#include <iostream>

#include "tradecore/api/api_router.hpp"
#include "tradecore/funding/funding_engine.hpp"
#include "tradecore/ingest/ingress_pipeline.hpp"
#include "tradecore/ledger/ledger_state.hpp"
#include "tradecore/matcher/matching_engine.hpp"
#include "tradecore/replay/replay_driver.hpp"
#include "tradecore/risk/risk_engine.hpp"
#include "tradecore/snapshot/snapshot_store.hpp"
#include "tradecore/telemetry/telemetry_sink.hpp"
#include "tradecore/wal/wal_writer.hpp"

int main() {
  using namespace tradecore;

  ingest::IngressPipeline ingress;
  ingress.configure("quic://127.0.0.1:9000");

  matcher::MatchingEngine matcher;
  matcher.initialize(1);

  risk::RiskEngine risk;
  risk.bootstrap();

  funding::FundingEngine funding;
  funding.configure(25);

  ledger::LedgerState ledger;
  ledger.credit(1, 1'000'000);

  wal::WalWriter wal;
  wal.open("/tmp/tradecore.wal");

  snapshot::SnapshotStore snapshot;
  snapshot.prepare("/tmp/tradecore.snapshot");

  replay::ReplayDriver replay;
  replay.load(0, 0);

  telemetry::TelemetrySink telemetry;
  telemetry.push({.id = 1, .value = 42});

  api::ApiRouter api;
  api.register_endpoint("/orders");

  std::cout << "tradecored skeleton bootstrapped" << std::endl;
  return 0;
}
