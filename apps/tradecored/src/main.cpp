#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#include "tradecore/api/api_router.hpp"
#include "tradecore/common/time_utils.hpp"
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

int main() {
  using namespace tradecore;

  std::cout << "Initializing TradeCore production engine..." << std::endl;

  ingest::IngressPipeline ingress;
  ingest::IngressPipeline::Config ingress_cfg;
  ingress_cfg.max_new_orders_per_second = 100'000;
  ingress.configure(ingress_cfg);

  ingest::QuicTransport transport;
  transport.start("quic://127.0.0.1:9000", [&](const ingest::Frame& frame) {
    ingress.submit(frame);
  });

  matcher::MatchingEngine matcher;
  matcher.add_market(1);

  risk::RiskEngine risk;
  risk.configure_market(1, {.contract_size = 1, .initial_margin_basis_points = 500, .maintenance_margin_basis_points = 300});
  risk.set_mark_price(1, 1000);
  risk.credit_collateral(1, 1'000'000);

  funding::FundingEngine funding;
  funding.configure_market(1, {.clamp_basis_points = 50, .max_rate_basis_points = 100});
  funding.update_market(1, 1'000, 1'005, 1);

  funding::FundingApplicator funding_applicator{funding, risk};
  risk::LiquidationExecutor liquidation_executor{risk, matcher};

  ledger::LedgerState ledger;
  ledger.credit(1, 1'000'000);

  const auto wal_path = std::filesystem::path{"/tmp/tradecore.wal"};
  wal::Writer wal{wal_path};

  const auto snapshot_dir = std::filesystem::path{"/tmp/tradecore.snapshot"};
  snapshot::Store snapshot{snapshot_dir};

  replay::Driver replay;
  replay.configure(snapshot.directory(), wal_path);
  replay.set_event_handler([](const wal::Record&) {});

  telemetry::TelemetrySink telemetry;
  telemetry.push({.id = 1, .value = 42});

  api::ApiRouter api;
  api.register_endpoint("/orders");
  api.register_endpoint("/express-feed");
  api.register_endpoint("/trade-metadata");
  api.register_endpoint("/state-root");

  std::cout << "TradeCore production engine initialized" << std::endl;
  std::cout << "API endpoints registered: /orders, /express-feed, /trade-metadata, /state-root" << std::endl;
  std::cout << "QUIC transport listening on 127.0.0.1:9000" << std::endl;

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
          .wal_offset = wal_offset++,
          .quantity = fill.quantity,
          .price = fill.price,
          .side = decoded.side
      };
      api.push_trade_metadata(metadata);
    }

    api::ExpressFeedFrame express_frame{
        .sequence = event_sequence++,
        .wal_offset = wal_offset++,
        .payload = std::vector<std::byte>(frame.payload.begin(), frame.payload.end())
    };
    api.push_express_feed_frame(express_frame);
  }

  auto accounts = risk.get_all_accounts();
  auto liquidations = liquidation_executor.check_and_liquidate_accounts(accounts);
  std::cout << "Processed " << liquidations.size() << " liquidations" << std::endl;

  std::vector<common::MarketId> markets = {1};
  auto funding_payments = funding_applicator.apply_funding(markets);
  std::cout << "Applied " << funding_payments.size() << " funding payments" << std::endl;

  api::StateRoot state_root{
      .sequence = event_sequence,
      .merkle_root = {},
      .timestamp_ns = 0
  };
  api.set_state_root(state_root);

  std::cout << "TradeCore production engine ready for shutdown" << std::endl;
  transport.stop();
  return 0;
}
