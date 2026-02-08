#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tradecore/api/api_router.hpp"
#include "tradecore/auth/authenticator.hpp"
#include "tradecore/config/config_loader.hpp"
#include "tradecore/funding/funding_engine.hpp"
#include "tradecore/ingest/ingress_pipeline.hpp"
#include "tradecore/ingest/quic_transport.hpp"
#include "tradecore/ingest/sbe_messages.hpp"
#include "tradecore/ledger/ledger_state.hpp"
#include "tradecore/matcher/matching_engine.hpp"
#include "tradecore/replay/replay_driver.hpp"
#include "tradecore/risk/liquidation_engine.hpp"
#include "tradecore/risk/risk_engine.hpp"
#include "tradecore/snapshot/snapshot_store.hpp"
#include "tradecore/telemetry/telemetry_sink.hpp"
#include "tradecore/wal/wal_writer.hpp"

namespace {

std::atomic<bool> g_shutdown_requested{false};

struct RestingOrderContext {
  tradecore::common::AccountId account{0};
  tradecore::common::MarketId market{0};
  tradecore::common::Side side{tradecore::common::Side::kBuy};
};

void print_usage(const char* program) {
  std::cerr << "Usage: " << program << " [config_file]\n"
            << "  config_file: Path to TOML configuration file\n"
            << "               If not specified, uses ./tradecore.toml or generates defaults\n";
}

void handle_shutdown_signal(int /*signal*/) {
  g_shutdown_requested.store(true);
}

template <typename T>
void append_primitive(std::vector<std::byte>& payload, T value) {
  auto raw = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
  payload.insert(payload.end(), raw.begin(), raw.end());
}

std::vector<std::byte> build_wal_payload(const tradecore::ingest::OwnedFrame& frame) {
  std::vector<std::byte> payload;
  payload.reserve(
      sizeof(std::uint8_t) + sizeof(frame.header.account) + sizeof(frame.header.nonce) +
      sizeof(frame.header.received_time_ns) + frame.payload.size());
  append_primitive<std::uint8_t>(payload, static_cast<std::uint8_t>(frame.header.kind));
  append_primitive<tradecore::common::AccountId>(payload, frame.header.account);
  append_primitive<std::uint64_t>(payload, frame.header.nonce);
  append_primitive<tradecore::common::TimestampNs>(payload, frame.header.received_time_ns);
  payload.insert(payload.end(), frame.payload.begin(), frame.payload.end());
  return payload;
}

std::uint64_t append_ingress_wal_record(tradecore::wal::Writer& wal, const tradecore::ingest::OwnedFrame& frame) {
  auto payload = build_wal_payload(frame);
  const auto wal_offset = wal.next_sequence();
  wal.append({
      .payload = std::span<const std::byte>(payload.data(), payload.size()),
  });
  return wal_offset;
}

tradecore::common::OrderId decode_order_id(std::uint64_t encoded) {
  return tradecore::common::OrderId{
      .market = static_cast<tradecore::common::MarketId>(encoded >> 48),
      .session = static_cast<tradecore::common::SessionId>((encoded >> 32) & 0xffff),
      .local = static_cast<tradecore::common::SequenceId>(encoded & 0xffffffff),
  };
}

tradecore::common::Side opposite_side(tradecore::common::Side side) {
  return side == tradecore::common::Side::kBuy ? tradecore::common::Side::kSell : tradecore::common::Side::kBuy;
}

std::filesystem::path find_config_path(int argc, char* argv[]) {
  if (argc > 1) {
    return std::filesystem::path{argv[1]};
  }

  std::filesystem::path default_paths[] = {
      "./tradecore.toml",
      "/etc/tradecore/tradecore.toml",
      std::filesystem::path{getenv("HOME") ? getenv("HOME") : ""} / ".config/tradecore/tradecore.toml",
  };

  for (const auto& path : default_paths) {
    if (!path.empty() && std::filesystem::exists(path)) {
      return path;
    }
  }

  return {};
}

}  // namespace

int main(int argc, char* argv[]) {
  using namespace tradecore;

  if (argc > 2) {
    print_usage(argv[0]);
    return 1;
  }

  auto config_path = find_config_path(argc, argv);
  config::EngineConfig cfg;

  if (config_path.empty()) {
    std::cout << "No config file found, using defaults\n";
    auto result = config::ConfigLoader::load_from_string(config::ConfigLoader::generate_default());
    if (!result.success) {
      std::cerr << "Failed to load default config: " << result.raw_error << "\n";
      return 1;
    }
    cfg = std::move(result.config);
  } else {
    std::cout << "Loading config from: " << config_path << "\n";
    auto result = config::ConfigLoader::load(config_path);
    if (!result.success) {
      if (!result.raw_error.empty()) {
        std::cerr << "Parse error: " << result.raw_error << "\n";
      }
      for (const auto& err : result.errors) {
        std::cerr << "Validation error [" << err.field << "]: " << err.message << "\n";
      }
      return 1;
    }
    cfg = std::move(result.config);
  }

  std::cout << "Config loaded successfully\n";
  std::cout << "  Transport endpoint: " << cfg.transport.endpoint << "\n";
  std::cout << "  Markets: " << cfg.markets.size() << "\n";
  std::cout << "  WAL path: " << cfg.persistence.wal_path << "\n";

  // Initialize authenticator for ed25519 signature verification
  auth::Authenticator authenticator;

  // TODO: Load account public keys from config or key store
  // For now, generate a test keypair for development
  auth::PublicKey test_pubkey;
  auth::SecretKey test_seckey;
  auth::Authenticator::generate_keypair(test_pubkey, test_seckey);
  authenticator.register_account(1, test_pubkey);  // Register test account

  std::cout << "  Auth: " << authenticator.account_count() << " registered accounts\n";

  // Create frame authenticator for signature verification
  auth::FrameAuthenticator frame_auth(authenticator);

  // Create auth verifier callback for ingress pipeline
  auto auth_verifier = [&frame_auth](const ingest::FrameHeader& header,
                                      std::span<const std::byte> payload) -> bool {
    // Verify the frame signature
    // Note: In production, the header bytes would come from the wire format
    return frame_auth.verify_frame(&header, sizeof(header), payload, header.account);
  };

  ingest::IngressPipeline ingress;
  ingest::IngressPipeline::Config ingress_cfg;
  ingress_cfg.max_new_orders_per_second = cfg.ingress.max_new_orders_per_second;
  ingress_cfg.max_cancels_per_second = cfg.ingress.max_cancels_per_second;
  ingress_cfg.new_order_queue_depth = cfg.ingress.new_order_queue_depth;
  ingress_cfg.cancel_queue_depth = cfg.ingress.cancel_queue_depth;
  ingress_cfg.replace_queue_depth = cfg.ingress.replace_queue_depth;
  ingress.configure(ingress_cfg, auth_verifier);

  ingest::QuicTransport transport;
  if (!transport.start(cfg.transport.endpoint, [&](const ingest::Frame& frame) {
    ingress.submit(frame);
  })) {
    std::cerr << "Failed to start transport on " << cfg.transport.endpoint << "\n";
    return 1;
  }

  matcher::MatchingEngine matcher;
  risk::RiskEngine risk;
  funding::FundingEngine funding;

  for (const auto& market_cfg : cfg.markets) {
    std::cout << "  Configuring market " << market_cfg.id << " (" << market_cfg.symbol << ")\n";

    matcher.add_market(market_cfg.id);

    risk.configure_market(market_cfg.id, {
        .contract_size = market_cfg.risk.contract_size,
        .initial_margin_basis_points = market_cfg.risk.initial_margin_basis_points,
        .maintenance_margin_basis_points = market_cfg.risk.maintenance_margin_basis_points,
    });
    risk.set_mark_price(market_cfg.id, market_cfg.risk.initial_mark_price);

    funding.configure_market(market_cfg.id, {
        .clamp_basis_points = market_cfg.funding.clamp_basis_points,
        .max_rate_basis_points = market_cfg.funding.max_rate_basis_points,
    });
  }

  risk::LiquidationManager liquidation{risk};
  (void)liquidation;

  ledger::LedgerState ledger;
  (void)ledger;

  std::filesystem::create_directories(cfg.persistence.snapshot_dir);
  std::filesystem::create_directories(cfg.persistence.wal_path.parent_path());

  wal::Writer wal{cfg.persistence.wal_path, cfg.persistence.wal_flush_threshold};
  snapshot::Store snapshot{cfg.persistence.snapshot_dir};

  replay::Driver replay;
  replay.configure(snapshot.directory(), cfg.persistence.wal_path);
  (void)replay;

  telemetry::TelemetrySink telemetry;
  if (cfg.telemetry.enabled) {
    telemetry.push({.id = 1, .value = 0});
  }

  api::ApiRouter api;
  api.register_endpoint("/orders");
  api.register_endpoint("/express-feed");
  api.register_endpoint("/trade-metadata");
  api.register_endpoint("/state-root");

  std::atomic<std::uint64_t> chain_id{1};
  if (const char* env_chain_id = std::getenv("MONMOUTH_CHAIN_ID")) {
    try {
      chain_id.store(std::stoull(env_chain_id, nullptr, 0));
    } catch (const std::exception&) {
      std::cerr << "Invalid MONMOUTH_CHAIN_ID value: " << env_chain_id << "\n";
      return 1;
    }
  }
  std::atomic<std::uint64_t> block_number{0};

  api.set_node_state_provider({
      .chain_id = [&chain_id]() { return chain_id.load(); },
      .block_number = [&block_number]() { return block_number.load(); },
      .peer_connections = [&transport]() { return transport.stats().connections_active; },
      .healthy = [&transport]() { return transport.is_running(); },
  });

  std::cout << "  RPC eth_chainId: " << api.rpc_result("eth_chainId") << "\n";
  std::cout << "  RPC eth_blockNumber: " << api.rpc_result("eth_blockNumber") << "\n";

  const common::MarketId default_market = cfg.markets.empty()
                                              ? common::MarketId{1}
                                              : static_cast<common::MarketId>(cfg.markets.front().id);
  std::unordered_map<std::uint64_t, RestingOrderContext> resting_orders{};

  auto process_fills = [&](const std::vector<matcher::FillEvent>& fills,
                           const RestingOrderContext& taker,
                           std::uint64_t wal_offset,
                           common::TimestampNs timestamp_ns) {
    for (const auto& fill : fills) {
      risk.apply_fill({
          .account = taker.account,
          .market = taker.market,
          .side = taker.side,
          .quantity = fill.quantity,
          .price = fill.price,
      });

      if (const auto maker_it = resting_orders.find(fill.maker_order.value());
          maker_it != resting_orders.end()) {
        const auto& maker = maker_it->second;
        risk.apply_fill({
            .account = maker.account,
            .market = maker.market,
            .side = maker.side,
            .quantity = fill.quantity,
            .price = fill.price,
        });
      }

      api.push_trade_metadata({
          .wal_offset = wal_offset,
          .order_id = fill.taker_order,
          .account = taker.account,
          .market = taker.market,
          .price = fill.price,
          .quantity = fill.quantity,
          .timestamp_ns = timestamp_ns,
      });
    }
  };

  auto process_new_orders = [&]() -> std::uint64_t {
    std::uint64_t processed{0};
    ingest::OwnedFrame frame;
    while (ingress.next_new_order(frame)) {
      ++processed;
      const auto wal_offset = append_ingress_wal_record(wal, frame);
      api.push_express_feed_frame({
          .wal_offset = wal_offset,
          .payload = frame.payload,
      });

      try {
        const auto order = ingest::sbe::decode_new_order(frame.payload);
        const common::OrderId order_id{
            .market = default_market,
            .session = static_cast<common::SessionId>(frame.header.account & 0xffff),
            .local = static_cast<common::SequenceId>(frame.header.nonce & 0xffffffff),
        };

        const auto reduce_only = common::HasFlag(order.flags, common::OrderFlags::kReduceOnly);
        const auto risk_result = risk.evaluate_order({
            .account = frame.header.account,
            .market = default_market,
            .side = order.side,
            .quantity = order.quantity,
            .limit_price = order.price,
            .reduce_only = reduce_only,
        });
        if (risk_result.decision != risk::Decision::kAccepted) {
          continue;
        }

        const auto result = matcher.submit({
            .id = order_id,
            .account = frame.header.account,
            .side = order.side,
            .quantity = order.quantity,
            .price = order.price,
            .tif = common::TimeInForce::kGtc,
            .flags = order.flags,
        });
        if (!result.accepted) {
          continue;
        }

        const RestingOrderContext taker{
            .account = frame.header.account,
            .market = default_market,
            .side = order.side,
        };
        process_fills(result.fills, taker, wal_offset, frame.header.received_time_ns);

        if (result.resting) {
          resting_orders[order_id.value()] = taker;
        } else {
          resting_orders.erase(order_id.value());
        }
      } catch (const std::exception& ex) {
        std::cerr << "Failed to process new order: " << ex.what() << "\n";
      }
    }
    return processed;
  };

  auto process_cancels = [&]() -> std::uint64_t {
    std::uint64_t processed{0};
    ingest::OwnedFrame frame;
    while (ingress.next_cancel(frame)) {
      ++processed;
      const auto wal_offset = append_ingress_wal_record(wal, frame);
      api.push_express_feed_frame({
          .wal_offset = wal_offset,
          .payload = frame.payload,
      });

      try {
        const auto cancel = ingest::sbe::decode_cancel(frame.payload);
        const auto order_id = decode_order_id(cancel.order_id);
        const auto result = matcher.cancel({.id = order_id});
        if (result.cancelled) {
          resting_orders.erase(order_id.value());
        }
      } catch (const std::exception& ex) {
        std::cerr << "Failed to process cancel: " << ex.what() << "\n";
      }
    }
    return processed;
  };

  auto process_replaces = [&]() -> std::uint64_t {
    std::uint64_t processed{0};
    ingest::OwnedFrame frame;
    while (ingress.next_replace(frame)) {
      ++processed;
      const auto wal_offset = append_ingress_wal_record(wal, frame);
      api.push_express_feed_frame({
          .wal_offset = wal_offset,
          .payload = frame.payload,
      });

      try {
        const auto replace = ingest::sbe::decode_replace(frame.payload);
        const auto order_id = decode_order_id(replace.order_id);

        const auto taker_it = resting_orders.find(order_id.value());
        RestingOrderContext taker = {
            .account = frame.header.account,
            .market = order_id.market,
            .side = common::Side::kBuy,
        };
        if (taker_it != resting_orders.end()) {
          taker = taker_it->second;
        }

        const auto result = matcher.replace({
            .id = order_id,
            .new_quantity = replace.new_quantity,
            .new_price = replace.new_price,
            .new_flags = replace.new_flags,
        });

        if (result.accepted) {
          process_fills(result.fills, taker, wal_offset, frame.header.received_time_ns);
        }

        if (result.accepted && result.resting) {
          resting_orders[order_id.value()] = taker;
        } else if (result.accepted && !result.resting) {
          resting_orders.erase(order_id.value());
        }
      } catch (const std::exception& ex) {
        std::cerr << "Failed to process replace: " << ex.what() << "\n";
      }
    }
    return processed;
  };

  std::signal(SIGINT, handle_shutdown_signal);
  std::signal(SIGTERM, handle_shutdown_signal);

  std::cout << "tradecored bootstrapped successfully\n";
  std::cout << "Entering event loop. Press Ctrl+C to shut down.\n";

  constexpr auto kIdleSleep = std::chrono::milliseconds(10);
  constexpr auto kStatusInterval = std::chrono::seconds(1);
  constexpr std::uint64_t kSnapshotInterval = 256;
  auto last_status = std::chrono::steady_clock::now();
  std::uint64_t last_snapshot_block = 0;

  while (!g_shutdown_requested.load()) {
    const auto processed = process_new_orders() + process_cancels() + process_replaces();
    if (processed > 0) {
      const auto new_block = block_number.fetch_add(processed) + processed;
      if (new_block - last_snapshot_block >= kSnapshotInterval) {
        std::vector<std::byte> snapshot_payload;
        snapshot_payload.reserve(sizeof(std::uint64_t) * 2);
        append_primitive<std::uint64_t>(snapshot_payload, chain_id.load());
        append_primitive<std::uint64_t>(snapshot_payload, new_block);
        const auto seq = static_cast<common::SequenceId>(
            std::min<std::uint64_t>(new_block, std::numeric_limits<common::SequenceId>::max()));
        snapshot.persist(seq, std::span<const std::byte>(snapshot_payload.data(), snapshot_payload.size()));
        last_snapshot_block = new_block;
      }
    } else {
      std::this_thread::sleep_for(kIdleSleep);
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - last_status >= kStatusInterval) {
      const auto stats = transport.stats();
      std::cout << "[status] block=" << block_number.load()
                << " ingress_accepted=" << ingress.stats().accepted
                << " frames=" << stats.frames_received
                << " peers=" << stats.connections_active
                << " wal_next=" << wal.next_sequence() << "\n";
      last_status = now;
    }
  }

  std::cout << "Shutdown signal received, flushing state...\n";
  transport.stop();
  wal.sync();
  return 0;
}
