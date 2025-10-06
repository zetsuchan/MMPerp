#include <array>
#include <cassert>
#include <span>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <vector>

#include "tradecore/api/api_router.hpp"
#include "tradecore/funding/funding_engine.hpp"
#include "tradecore/ingest/ingress_pipeline.hpp"
#include "tradecore/ingest/sbe_messages.hpp"
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
  risk.configure_market(1, {.contract_size = 1, .initial_margin_basis_points = 500, .maintenance_margin_basis_points = 300});
  risk.set_mark_price(1, 1'000);
  risk.credit_collateral(1'001, 30'000);

  risk::OrderIntent open_intent{
      .account = 1'001,
      .market = 1,
      .side = common::Side::kBuy,
      .quantity = 400,
      .limit_price = 1'000,
      .reduce_only = false,
  };
  auto open_eval = risk.evaluate_order(open_intent);
  assert(open_eval.decision == risk::Decision::kAccepted);

  risk.apply_fill({.account = 1'001, .market = 1, .side = common::Side::kBuy, .quantity = 400, .price = 1'000});

  risk.set_mark_price(1, 960);
  risk::LiquidationManager liquidation{risk};
  auto partial_liq = liquidation.evaluate(1'001);
  assert(partial_liq.status == risk::LiquidationManager::Status::kNeedsPartial);

  auto reduce_eval = risk.evaluate_order({
      .account = 1'001,
      .market = 1,
      .side = common::Side::kBuy,
      .quantity = 10,
      .limit_price = 950,
      .reduce_only = true,
  });
  assert(reduce_eval.decision == risk::Decision::kRejectedReduceOnly);

  risk.set_mark_price(1, 900);
  auto full_liq = liquidation.evaluate(1'001);
  assert(full_liq.status == risk::LiquidationManager::Status::kNeedsFull);

  telemetry::TelemetrySink telemetry;
  telemetry.push({.id = 1, .value = 99});
  auto samples = telemetry.drain();
  assert(samples.size() == 1);
  assert(samples.front().value == 99);

  ingest::IngressPipeline pipeline;
  ingest::IngressPipeline::Config ingress_cfg;
  ingress_cfg.max_new_orders_per_second = 2;
  pipeline.configure(ingress_cfg);

  ingest::sbe::NewOrder sbe_new{
      .side = common::Side::kBuy,
      .quantity = 5,
      .price = 1'000,
      .flags = 0,
  };
  auto payload_new = ingest::sbe::encode(sbe_new);
  ingest::Frame new_frame{
      .header = {.account = 9, .nonce = 1, .received_time_ns = 0, .priority = 0, .kind = ingest::MessageKind::kNewOrder},
      .payload = std::span<const std::byte>(payload_new.data(), payload_new.size()),
  };
  assert(pipeline.submit(new_frame));

  ingest::OwnedFrame dequeued;
  assert(pipeline.next_new_order(dequeued));
  const auto decoded_new = ingest::sbe::decode_new_order(dequeued.payload);
  assert(decoded_new.quantity == sbe_new.quantity);

  ingest::sbe::Cancel sbe_cancel{.order_id = 42};
  auto payload_cancel = ingest::sbe::encode(sbe_cancel);
  ingest::Frame cancel_frame{
      .header = {.account = 9, .nonce = 2, .received_time_ns = 0, .priority = 0, .kind = ingest::MessageKind::kCancel},
      .payload = std::span<const std::byte>(payload_cancel.data(), payload_cancel.size()),
  };
  assert(pipeline.submit(cancel_frame));
  ingest::OwnedFrame dequeued_cancel;
  assert(pipeline.next_cancel(dequeued_cancel));
  const auto decoded_cancel = ingest::sbe::decode_cancel(dequeued_cancel.payload);
  assert(decoded_cancel.order_id == 42);

  ingest::Frame heartbeat{
      .header = {.account = 9, .nonce = 3, .received_time_ns = 0, .priority = 0, .kind = ingest::MessageKind::kHeartbeat},
      .payload = std::span<const std::byte>(),
  };
  assert(pipeline.submit(heartbeat));
  assert(pipeline.stats().dropped_heartbeats == 1);

  // rate limit: third new order in same window rejected
  assert(pipeline.submit(new_frame));
  auto second_payload = ingest::sbe::encode(sbe_new);
  ingest::Frame rate_frame{
      .header = {.account = 9, .nonce = 4, .received_time_ns = 0, .priority = 0, .kind = ingest::MessageKind::kNewOrder},
      .payload = std::span<const std::byte>(second_payload.data(), second_payload.size()),
  };
  assert(!pipeline.submit(rate_frame));
  assert(pipeline.stats().rejected_rate_limit == 1);

  funding::FundingEngine funding;
  funding.configure_market(1, {.clamp_basis_points = 50, .max_rate_basis_points = 100});
  const auto funding_snapshot = funding.update_market(1, 1'000, 1'020, 1);
  assert(funding_snapshot.mark_price == 1'005);
  assert(funding_snapshot.premium_rate == 50);
  assert(funding.accumulated_funding(1) == 50);

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

  // Persistence + replay deterministic harness
  namespace fs = std::filesystem;
  const auto tmp_root = fs::temp_directory_path() / "tradecore_tests";
  fs::remove_all(tmp_root);

  snapshot::Store snapshot_store(tmp_root);
  struct SnapshotState {
    std::int64_t balance;
  } snapshot_state{.balance = 42};
  snapshot_store.persist(0, std::as_bytes(std::span(&snapshot_state, 1)));
  auto snap = snapshot_store.latest();
  assert(snap.has_value());
  assert(snap->sequence == 0);

  const auto wal_path = tmp_root / "events.wal";
  wal::Writer writer(wal_path, 128);

  auto append_int = [&](std::int32_t value) {
    std::array<std::byte, sizeof(value)> payload{};
    std::memcpy(payload.data(), &value, sizeof(value));
    wal::RecordView rec{.header = {}, .payload = std::span<const std::byte>(payload.data(), payload.size())};
    writer.append(rec);
  };

  append_int(10);
  append_int(-5);
  writer.sync();

  replay::Driver driver;
  driver.configure(tmp_root, wal_path);

  std::int64_t replay_balance = 0;
  driver.set_snapshot_handler([&](common::SequenceId seq, std::span<const std::byte> payload) {
    assert(seq == 0);
    assert(payload.size() == sizeof(SnapshotState));
    std::memcpy(&replay_balance, payload.data(), payload.size());
  });

  driver.set_event_handler([&](const wal::Record& record) {
    std::int32_t delta = 0;
    if (!record.payload.empty()) {
      std::memcpy(&delta, record.payload.data(), sizeof(delta));
    }
    replay_balance += delta;
  });

  driver.execute();
  assert(replay_balance == 47);

  fs::remove_all(tmp_root);

  return 0;
}
