#include <cassert>
#include <cstddef>
#include <vector>

#include "tradecore/api/api_router.hpp"
#include "tradecore/funding/funding_engine.hpp"
#include "tradecore/ingest/ingress_pipeline.hpp"
#include "tradecore/ledger/ledger_state.hpp"
#include "tradecore/matcher/matching_engine.hpp"
#include "tradecore/risk/risk_engine.hpp"
#include "tradecore/telemetry/telemetry_sink.hpp"

int main() {
  using namespace tradecore;

  api::ApiRouter router;
  router.register_endpoint("/orders");
  assert(router.has_endpoint("/orders"));

  ledger::LedgerState ledger;
  ledger.credit(7, 100);
  ledger.debit(7, 10);
  const auto account = ledger.get(7);
  assert(account.collateral_available == 90);
  assert(account.collateral_locked == 10);

  risk::RiskEngine risk;
  risk.bootstrap();
  const auto risk_result = risk.evaluate_order({1, 1, 1}, 42);
  assert(risk_result.decision == risk::Decision::kAccepted);
  assert(risk_result.margin_requirement == 42);

  telemetry::TelemetrySink telemetry;
  telemetry.push({.id = 1, .value = 99});
  auto samples = telemetry.drain();
  assert(samples.size() == 1);
  assert(samples.front().value == 99);

  ingest::IngressPipeline ingress;
  ingress.configure("quic://0.0.0.0:9000");
  std::byte payload[]{std::byte{0x01}};
  ingest::FrameView frame{.payload = std::span<const std::byte>(payload), .received_time_ns = 0};
  assert(ingress.enqueue(frame));

  funding::FundingEngine funding;
  funding.configure(25);
  const auto snapshot = funding.compute_snapshot(1);
  assert(snapshot.premium_rate == 25);

  matcher::MatchingEngine matcher;
  matcher.add_market(1);

  common::OrderId maker_id{.market = 1, .session = 1, .local = 1};
  common::OrderId taker_id{.market = 1, .session = 1, .local = 2};

  matcher::OrderRequest maker{
      .id = maker_id,
      .account = 1001,
      .side = common::Side::kSell,
      .quantity = 5,
      .price = 1000,
      .tif = common::TimeInForce::kGtc,
      .flags = common::kFlagsNone,
  };
  auto maker_res = matcher.submit(maker);
  assert(maker_res.accepted);
  assert(maker_res.resting);

  matcher::OrderRequest taker{
      .id = taker_id,
      .account = 1002,
      .side = common::Side::kBuy,
      .quantity = 3,
      .price = 1100,
      .tif = common::TimeInForce::kIoc,
      .flags = common::kFlagsNone,
  };
  auto taker_res = matcher.submit(taker);
  assert(taker_res.accepted);
  assert(!taker_res.resting);
  assert(!taker_res.fills.empty());
  assert(taker_res.fills.front().quantity == 3);

  matcher::CancelRequest cancel_req{.id = maker_id};
  auto cancel_res = matcher.cancel(cancel_req);
  assert(cancel_res.cancelled);

  return 0;
}
