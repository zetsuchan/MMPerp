#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

namespace tradecore {
namespace telemetry {

struct Sample {
  std::uint64_t id{};
  std::int64_t value{};
};

// Streaming histogram with O(1) recording and O(bucket_count) percentile computation.
// Uses log2-scale buckets from 1ns to ~1s (30 buckets).
class StreamingHistogram {
 public:
  static constexpr std::size_t kNumBuckets = 30;  // 1ns to ~1 billion ns (~1s)

  void record(std::int64_t value_ns) noexcept;
  void reset() noexcept;

  [[nodiscard]] std::uint64_t count() const noexcept { return count_; }
  [[nodiscard]] double mean() const noexcept;
  [[nodiscard]] double percentile(double p) const noexcept;

 private:
  std::array<std::uint64_t, kNumBuckets> buckets_{};
  std::uint64_t count_{0};
  std::int64_t sum_{0};
  std::int64_t min_{std::numeric_limits<std::int64_t>::max()};
  std::int64_t max_{0};

  static std::size_t bucket_index(std::int64_t value_ns) noexcept;
  static std::int64_t bucket_midpoint(std::size_t idx) noexcept;
};

class TelemetrySink {
 public:
  void push(Sample sample);
  void increment(std::uint64_t id, std::int64_t delta = 1);
  void record_latency(std::uint64_t id, std::chrono::nanoseconds latency);
  [[nodiscard]] std::vector<Sample> drain();

  struct Summary {
    std::uint64_t id{0};
    std::uint64_t count{0};
    double mean_ns{0.0};
    double p99_ns{0.0};
  };

  [[nodiscard]] std::vector<Summary> drain_latency();

 private:
  static constexpr std::size_t kMaxMetricId = 1024;

  std::mutex mutex_;
  std::vector<Sample> buffer_{};
  std::array<StreamingHistogram, kMaxMetricId> histograms_{};
};

}  // namespace telemetry
}  // namespace tradecore
