#include "tradecore/telemetry/telemetry_sink.hpp"

#include <bit>
#include <limits>

namespace tradecore {
namespace telemetry {

// StreamingHistogram implementation

std::size_t StreamingHistogram::bucket_index(std::int64_t value_ns) noexcept {
  if (value_ns <= 0) {
    return 0;
  }
  // Use bit_width to find log2 bucket: bucket[i] covers [2^i, 2^(i+1))
  const auto bits = std::bit_width(static_cast<std::uint64_t>(value_ns));
  return std::min(static_cast<std::size_t>(bits), kNumBuckets - 1);
}

std::int64_t StreamingHistogram::bucket_midpoint(std::size_t idx) noexcept {
  if (idx == 0) {
    return 1;
  }
  // Midpoint of [2^(idx-1), 2^idx) is approximately 1.5 * 2^(idx-1)
  return static_cast<std::int64_t>(3) << (idx - 2);  // 3 * 2^(idx-2) = 1.5 * 2^(idx-1)
}

void StreamingHistogram::record(std::int64_t value_ns) noexcept {
  const auto idx = bucket_index(value_ns);
  ++buckets_[idx];
  ++count_;
  sum_ += value_ns;
  min_ = std::min(min_, value_ns);
  max_ = std::max(max_, value_ns);
}

void StreamingHistogram::reset() noexcept {
  buckets_.fill(0);
  count_ = 0;
  sum_ = 0;
  min_ = std::numeric_limits<std::int64_t>::max();
  max_ = 0;
}

double StreamingHistogram::mean() const noexcept {
  if (count_ == 0) {
    return 0.0;
  }
  return static_cast<double>(sum_) / static_cast<double>(count_);
}

double StreamingHistogram::percentile(double p) const noexcept {
  if (count_ == 0) {
    return 0.0;
  }

  const auto target = static_cast<std::uint64_t>(static_cast<double>(count_) * p);
  std::uint64_t cumulative = 0;

  for (std::size_t idx = 0; idx < kNumBuckets; ++idx) {
    cumulative += buckets_[idx];
    if (cumulative >= target) {
      return static_cast<double>(bucket_midpoint(idx));
    }
  }

  return static_cast<double>(max_);
}

// TelemetrySink implementation

void TelemetrySink::push(Sample sample) {
  std::scoped_lock lock(mutex_);
  buffer_.push_back(sample);
}

void TelemetrySink::increment(std::uint64_t id, std::int64_t delta) {
  push(Sample{.id = id, .value = delta});
}

void TelemetrySink::record_latency(std::uint64_t id, std::chrono::nanoseconds latency) {
  std::scoped_lock lock(mutex_);
  const auto idx = id % kMaxMetricId;
  histograms_[idx].record(latency.count());
}

std::vector<Sample> TelemetrySink::drain() {
  std::scoped_lock lock(mutex_);
  auto copy = std::move(buffer_);
  buffer_.clear();
  return copy;
}

std::vector<TelemetrySink::Summary> TelemetrySink::drain_latency() {
  std::scoped_lock lock(mutex_);
  std::vector<Summary> summaries;

  for (std::size_t idx = 0; idx < kMaxMetricId; ++idx) {
    auto& hist = histograms_[idx];
    if (hist.count() == 0) {
      continue;
    }

    summaries.push_back(Summary{
        .id = static_cast<std::uint64_t>(idx),
        .count = hist.count(),
        .mean_ns = hist.mean(),
        .p99_ns = hist.percentile(0.99),
    });

    hist.reset();
  }

  return summaries;
}

}  // namespace telemetry
}  // namespace tradecore
