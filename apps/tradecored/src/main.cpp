#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#include "tradecore/api/api_router.hpp"
#include "tradecore/auth/authenticator.hpp"
#include "tradecore/common/time_utils.hpp"
#include "tradecore/config/config_loader.hpp"
#include "tradecore/funding/funding_applicator.hpp"
#include "tradecore/funding/funding_engine.hpp"
#include "tradecore/ingest/ingress_pipeline.hpp"
#include "tradecore/ingest/quic_transport.hpp"
#include "tradecore/ingest/sbe_messages.hpp"
#include "tradecore/ledger/ledger_state.hpp"
#include "tradecore/matcher/matching_engine.hpp"
#include "tradecore/replay/replay_driver.hpp"
#include "tradecore/risk/liquidation_engine.hpp"
#include "tradecore/risk/liquidation_executor.hpp"
#include "tradecore/risk/risk_engine.hpp"
#include "tradecore/snapshot/snapshot_store.hpp"
#include "tradecore/telemetry/telemetry_sink.hpp"
#include "tradecore/wal/wal_writer.hpp"

namespace {

void print_usage(const char* program) {
  std::cerr << "Usage: " << program << " [config_file]\n"
            << "  config_file: Path to TOML configuration file\n"
            << "               If not specified, uses ./tradecore.toml or generates defaults\n";
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

  auth::Authenticator authenticator;

  auth::PublicKey test_pubkey;
  auth::SecretKey test_seckey;
  auth::Authenticator::generate_keypair(test_pubkey, test_seckey);
  authenticator.register_account(1, test_pubkey);

  std::cout << "  Auth: " << authenticator.account_count() << " registered accounts\n";

  auth::FrameAuthenticator frame_auth(authenticator);

  auto auth_verifier = [&frame_auth](const ingest::FrameHeader& header,
                                      std::span<const std::byte> payload) -> bool {
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
  transport.start(cfg.transport.endpoint, [&](const ingest::Frame& frame) {
    ingress.submit(frame);
  });

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

  funding::FundingApplicator funding_applicator{funding, risk};
  risk::LiquidationExecutor liquidation_executor{risk, matcher};
  risk::LiquidationManager liquidation{risk};

  ledger::LedgerState ledger;

  std::filesystem::create_directories(cfg.persistence.snapshot_dir);
  std::filesystem::create_directories(cfg.persistence.wal_path.parent_path());

  wal::Writer wal{cfg.persistence.wal_path, cfg.persistence.wal_flush_threshold};
  snapshot::Store snapshot{cfg.persistence.snapshot_dir};

  replay::Driver replay;
  replay.configure(snapshot.directory(), cfg.persistence.wal_path);

  telemetry::TelemetrySink telemetry;
  if (cfg.telemetry.enabled) {
    telemetry.push({.id = 1, .value = 0});
  }

  api::ApiRouter api;
  api.register_endpoint("/orders");
  api.register_endpoint("/express-feed");
  api.register_endpoint("/trade-metadata");
  api.register_endpoint("/state-root");

  std::cout << "tradecored bootstrapped successfully\n";
  std::cout << "API endpoints registered: /orders, /express-feed, /trade-metadata, /state-root\n";

  std::uint64_t event_sequence = 0;
  std::uint64_t wal_offset = 0;
  
  ingest::OwnedFrame frame;
  while (ingress.next_new_order(frame)) {
    auto decoded = ingest::sbe::decode_new_order(frame.payload);
    
    risk::OrderIntent intent{
        .account = frame.header.account,
        .market = 1,
        .side = decoded.side,
        .quantity = decoded.quantity,
        .limit_price = decoded.price,
        .reduce_only = false
    };
    
    auto risk_result = risk.evaluate_order(intent);
    if (risk_result.decision != risk::Decision::kAccepted) {
      continue;
    }

    common::OrderId order_id{.market = 1, .session = frame.header.account, .local = frame.header.nonce};
    matcher::OrderRequest order_request{
        .id = order_id,
        .account = frame.header.account,
        .side = decoded.side,
        .quantity = decoded.quantity,
        .price = decoded.price,
        .tif = common::TimeInForce::kGtc,
        .flags = decoded.flags
    };

    auto match_result = matcher.submit(order_request);
    
    const std::uint64_t this_wal_offset = wal_offset++;
    
    for (const auto& fill : match_result.fills) {
      risk::FillContext maker_fill{
          .account = fill.maker_order.session,
          .market = fill.maker_order.market,
          .side = (decoded.side == common::Side::kBuy) ? common::Side::kSell : common::Side::kBuy,
          .quantity = fill.quantity,
          .price = fill.price
      };
      risk.apply_fill(maker_fill);

      risk::FillContext taker_fill{
          .account = fill.taker_order.session,
          .market = fill.taker_order.market,
          .side = decoded.side,
          .quantity = fill.quantity,
          .price = fill.price
      };
      risk.apply_fill(taker_fill);

      api::TradeMetadata metadata{
          .order_id = fill.taker_order,
          .timestamp_ns = frame.header.received_time_ns,
          .wal_offset = this_wal_offset,
          .quantity = fill.quantity,
          .price = fill.price,
          .side = decoded.side
      };
      api.push_trade_metadata(metadata);
    }

    api::ExpressFeedFrame express_frame{
        .sequence = event_sequence++,
        .wal_offset = this_wal_offset,
        .payload = std::vector<std::byte>(frame.payload.begin(), frame.payload.end())
    };
    api.push_express_feed_frame(express_frame);
  }

  auto accounts = risk.get_all_accounts();
  auto liquidations = liquidation_executor.check_and_liquidate_accounts(accounts);
  std::cout << "Processed " << liquidations.size() << " liquidations\n";

  std::vector<common::MarketId> markets = {1};
  auto funding_payments = funding_applicator.apply_funding(markets);
  std::cout << "Applied " << funding_payments.size() << " funding payments\n";

  api::StateRoot state_root{
      .sequence = event_sequence,
      .merkle_root = {},
      .timestamp_ns = 0
  };
  api.set_state_root(state_root);

  transport.stop();
  return 0;
}
