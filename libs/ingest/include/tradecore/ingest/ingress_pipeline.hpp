#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <memory_resource>
#include <unordered_map>

#include "tradecore/common/spsc_ring.hpp"
#include "tradecore/ingest/frame.hpp"

namespace tradecore {
namespace ingest {

class IngressPipeline {
 public:
  struct Config {
    std::size_t new_order_queue_depth{1 << 12};
    std::size_t cancel_queue_depth{1 << 12};
    std::size_t replace_queue_depth{1 << 12};
    std::uint32_t max_new_orders_per_second{10'000};
    std::uint32_t max_cancels_per_second{20'000};
    std::uint32_t max_replaces_per_second{20'000};
  };

  struct Stats {
    std::uint64_t accepted{0};
    std::uint64_t rejected_auth{0};
    std::uint64_t rejected_rate_limit{0};
    std::uint64_t rejected_queue_full{0};
    std::uint64_t dropped_heartbeats{0};
  };

  using AuthVerifier = std::function<bool(const FrameHeader&, std::span<const std::byte>)>;

  IngressPipeline();

  void configure(const Config& config, AuthVerifier verifier = AuthVerifier{});
  bool submit(const Frame& frame);

  bool next_new_order(OwnedFrame& out);
  bool next_cancel(OwnedFrame& out);
  bool next_replace(OwnedFrame& out);

  [[nodiscard]] const Stats& stats() const noexcept { return stats_; }
  void reset_stats();

 private:
  struct AccountWindow {
    common::TimestampNs window_start{0};
    std::uint32_t new_orders{0};
    std::uint32_t cancels{0};
    std::uint32_t replaces{0};
  };

  Config config_{};
  AuthVerifier verifier_{};
  Stats stats_{};

  std::pmr::monotonic_buffer_resource arena_;
  std::pmr::unordered_map<common::AccountId, AccountWindow> rate_windows_;

  std::unique_ptr<common::SpscRing<OwnedFrame>> new_orders_;
  std::unique_ptr<common::SpscRing<OwnedFrame>> cancels_;
  std::unique_ptr<common::SpscRing<OwnedFrame>> replaces_;

  bool rate_limit(AccountWindow& window, MessageKind kind, common::TimestampNs timestamp);
};

}  // namespace ingest
}  // namespace tradecore
