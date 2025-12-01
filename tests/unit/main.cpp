// Unit test runner - calls test functions from per-component test files

#include "test_api.hpp"
#include "test_funding.hpp"
#include "test_ingest.hpp"
#include "test_ledger.hpp"
#include "test_matcher.hpp"
#include "test_persistence.hpp"
#include "test_risk.hpp"
#include "test_telemetry.hpp"

int main() {
  using namespace tradecore::tests;

  // API tests
  test_api_router();

  // Ledger tests
  test_ledger_credit_debit();

  // Risk tests
  test_risk_engine();
  test_liquidation();

  // Telemetry tests
  test_telemetry_sink();

  // Ingest tests
  test_ingress_pipeline();
  test_cancel_message();
  test_heartbeat_dropped();
  test_rate_limiting();

  // Funding tests
  test_funding_engine();

  // Matcher tests
  test_matching_engine();
  test_hidden_orders();
  test_iceberg_orders();
  test_iceberg_validation();

  // Persistence/replay tests
  test_persistence_replay();

  return 0;
}
