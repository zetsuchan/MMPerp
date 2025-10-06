#include <filesystem>
#include <iostream>

#include "tradecore/api/api_router.hpp"
#include "tradecore/funding/funding_engine.hpp"
#include "tradecore/ingest/ingress_pipeline.hpp"
#include "tradecore/ingest/quic_transport.hpp"
#include "tradecore/ledger/ledger_state.hpp"
#include "tradecore/matcher/matching_engine.hpp"
#include "tradecore/replay/replay_driver.hpp"
#include "tradecore/risk/liquidation_engine.hpp"
#include "tradecore/risk/risk_engine.hpp"
#include "tradecore/snapshot/snapshot_store.hpp"
#include "tradecore/telemetry/telemetry_sink.hpp"
#include "tradecore/wal/wal_writer.hpp"

int main() {
  using namespace tradecore;

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
  (void)funding.update_market(1, 1'000, 1'005, 1);

  risk::LiquidationManager liquidation{risk};
  auto liq_result = liquidation.evaluate(1);
  (void)liq_result;

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

  std::cout << "tradecored skeleton bootstrapped" << std::endl;
  transport.stop();
  return 0;
}
