#include "tradecore/ingest/ingress_pipeline.hpp"

#include <algorithm>
#include <chrono>
#include <memory>

namespace tradecore {
namespace ingest {

namespace {
constexpr common::TimestampNs kOneSecondNs = 1'000'000'000;
}

IngressPipeline::IngressPipeline()
    : arena_(1 << 16),
      rate_windows_(&arena_),
      new_orders_(std::make_unique<common::SpscRing<OwnedFrame>>(1 << 12)),
      cancels_(std::make_unique<common::SpscRing<OwnedFrame>>(1 << 12)),
      replaces_(std::make_unique<common::SpscRing<OwnedFrame>>(1 << 12)) {}

void IngressPipeline::configure(const Config& config, AuthVerifier verifier) {
  config_ = config;
  verifier_ = std::move(verifier);
  new_orders_ = std::make_unique<common::SpscRing<OwnedFrame>>(config.new_order_queue_depth);
  cancels_ = std::make_unique<common::SpscRing<OwnedFrame>>(config.cancel_queue_depth);
  replaces_ = std::make_unique<common::SpscRing<OwnedFrame>>(config.replace_queue_depth);
  rate_windows_.clear();
  stats_ = {};
}

bool IngressPipeline::submit(const Frame& frame) {
  if (frame.header.kind == MessageKind::kHeartbeat) {
    ++stats_.dropped_heartbeats;
    return true;
  }

  if (verifier_ && !verifier_(frame.header, frame.payload)) {
    ++stats_.rejected_auth;
    return false;
  }

  auto [it, inserted] = rate_windows_.try_emplace(frame.header.account, AccountWindow{});
  auto& window = it->second;
  if (rate_limit(window, frame.header.kind, frame.header.received_time_ns)) {
    ++stats_.rejected_rate_limit;
    return false;
  }

  OwnedFrame owned{
      .header = frame.header,
      .payload = std::vector<std::byte>(frame.payload.begin(), frame.payload.end()),
  };

  bool pushed = false;
  switch (frame.header.kind) {
    case MessageKind::kNewOrder:
      pushed = new_orders_->push(std::move(owned));
      break;
    case MessageKind::kCancel:
      pushed = cancels_->push(std::move(owned));
      break;
    case MessageKind::kReplace:
      pushed = replaces_->push(std::move(owned));
      break;
    case MessageKind::kHeartbeat:
      // handled earlier
      break;
  }

  if (!pushed) {
    ++stats_.rejected_queue_full;
    return false;
  }

  ++stats_.accepted;
  return true;
}

bool IngressPipeline::next_new_order(OwnedFrame& out) {
  return new_orders_->pop(out);
}

bool IngressPipeline::next_cancel(OwnedFrame& out) {
  return cancels_->pop(out);
}

bool IngressPipeline::next_replace(OwnedFrame& out) {
  return replaces_->pop(out);
}

void IngressPipeline::reset_stats() {
  stats_ = {};
}

bool IngressPipeline::rate_limit(AccountWindow& window, MessageKind kind, common::TimestampNs timestamp) {
  if (timestamp - window.window_start >= kOneSecondNs) {
    window.window_start = timestamp;
    window.new_orders = 0;
    window.cancels = 0;
    window.replaces = 0;
  }

  switch (kind) {
    case MessageKind::kNewOrder:
      if (window.new_orders >= config_.max_new_orders_per_second) {
        return true;
      }
      ++window.new_orders;
      break;
    case MessageKind::kCancel:
      if (window.cancels >= config_.max_cancels_per_second) {
        return true;
      }
      ++window.cancels;
      break;
    case MessageKind::kReplace:
      if (window.replaces >= config_.max_replaces_per_second) {
        return true;
      }
      ++window.replaces;
      break;
    case MessageKind::kHeartbeat:
      break;
  }

  return false;
}

}  // namespace ingest
}  // namespace tradecore
