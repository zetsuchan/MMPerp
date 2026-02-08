#include "test_ingest.hpp"

#include <cassert>
#include <stdexcept>
#include <span>
#include "tradecore/ingest/ingress_pipeline.hpp"
#include "tradecore/ingest/sbe_messages.hpp"

namespace tradecore::tests {

void test_ingress_pipeline() {
  ingest::IngressPipeline pipeline;
  ingest::IngressPipeline::Config cfg;
  cfg.max_new_orders_per_second = 2;
  pipeline.configure(cfg);

  ingest::sbe::NewOrder sbe_new{
      .side = common::Side::kBuy,
      .quantity = 5,
      .price = 1'000,
      .flags = 0,
  };
  auto payload_new = ingest::sbe::encode(sbe_new);
  ingest::Frame new_frame{
      .header = {.account = 9,
                 .nonce = 1,
                 .received_time_ns = 0,
                 .priority = 0,
                 .kind = ingest::MessageKind::kNewOrder},
      .payload = std::span<const std::byte>(payload_new.data(), payload_new.size()),
  };
  assert(pipeline.submit(new_frame));

  ingest::OwnedFrame dequeued;
  assert(pipeline.next_new_order(dequeued));
  const auto decoded_new = ingest::sbe::decode_new_order(dequeued.payload);
  assert(decoded_new.quantity == sbe_new.quantity);
}

void test_cancel_message() {
  ingest::IngressPipeline pipeline;
  pipeline.configure({});

  ingest::sbe::Cancel sbe_cancel{.order_id = 42};
  auto payload_cancel = ingest::sbe::encode(sbe_cancel);
  ingest::Frame cancel_frame{
      .header = {.account = 9,
                 .nonce = 2,
                 .received_time_ns = 0,
                 .priority = 0,
                 .kind = ingest::MessageKind::kCancel},
      .payload =
          std::span<const std::byte>(payload_cancel.data(), payload_cancel.size()),
  };
  assert(pipeline.submit(cancel_frame));

  ingest::OwnedFrame dequeued_cancel;
  assert(pipeline.next_cancel(dequeued_cancel));
  const auto decoded_cancel = ingest::sbe::decode_cancel(dequeued_cancel.payload);
  assert(decoded_cancel.order_id == 42);
}

void test_heartbeat_dropped() {
  ingest::IngressPipeline pipeline;
  pipeline.configure({});

  ingest::Frame heartbeat{
      .header = {.account = 9,
                 .nonce = 3,
                 .received_time_ns = 0,
                 .priority = 0,
                 .kind = ingest::MessageKind::kHeartbeat},
      .payload = std::span<const std::byte>(),
  };
  assert(pipeline.submit(heartbeat));
  assert(pipeline.stats().dropped_heartbeats == 1);
}

void test_rate_limiting() {
  ingest::IngressPipeline pipeline;
  ingest::IngressPipeline::Config cfg;
  cfg.max_new_orders_per_second = 2;
  pipeline.configure(cfg);

  ingest::sbe::NewOrder sbe_new{
      .side = common::Side::kBuy,
      .quantity = 5,
      .price = 1'000,
      .flags = 0,
  };

  // First two orders accepted
  auto payload1 = ingest::sbe::encode(sbe_new);
  ingest::Frame frame1{
      .header = {.account = 9,
                 .nonce = 1,
                 .received_time_ns = 0,
                 .priority = 0,
                 .kind = ingest::MessageKind::kNewOrder},
      .payload = std::span<const std::byte>(payload1.data(), payload1.size()),
  };
  assert(pipeline.submit(frame1));

  auto payload2 = ingest::sbe::encode(sbe_new);
  ingest::Frame frame2{
      .header = {.account = 9,
                 .nonce = 2,
                 .received_time_ns = 0,
                 .priority = 0,
                 .kind = ingest::MessageKind::kNewOrder},
      .payload = std::span<const std::byte>(payload2.data(), payload2.size()),
  };
  assert(pipeline.submit(frame2));

  // Third order in same window rejected
  auto payload3 = ingest::sbe::encode(sbe_new);
  ingest::Frame frame3{
      .header = {.account = 9,
                 .nonce = 3,
                 .received_time_ns = 0,
                 .priority = 0,
                 .kind = ingest::MessageKind::kNewOrder},
      .payload = std::span<const std::byte>(payload3.data(), payload3.size()),
  };
  assert(!pipeline.submit(frame3));
  assert(pipeline.stats().rejected_rate_limit == 1);
}

void test_sbe_decode_bounds() {
  {
    std::vector<std::byte> truncated(ingest::sbe::kNewOrderEncodedSize - 1);
    bool threw = false;
    try {
      (void)ingest::sbe::decode_new_order(truncated);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    assert(threw);
  }

  {
    std::vector<std::byte> truncated(ingest::sbe::kCancelEncodedSize - 1);
    bool threw = false;
    try {
      (void)ingest::sbe::decode_cancel(truncated);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    assert(threw);
  }

  {
    std::vector<std::byte> truncated(ingest::sbe::kReplaceEncodedSize - 1);
    bool threw = false;
    try {
      (void)ingest::sbe::decode_replace(truncated);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    assert(threw);
  }
}

}  // namespace tradecore::tests
