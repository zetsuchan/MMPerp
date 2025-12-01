#include "test_telemetry.hpp"

#include <cassert>
#include <chrono>
#include "tradecore/telemetry/telemetry_sink.hpp"

namespace tradecore::tests {

void test_telemetry_sink() {
  telemetry::TelemetrySink sink;
  sink.push({.id = 1, .value = 99});
  sink.increment(1, 2);
  sink.record_latency(1, std::chrono::nanoseconds{100});
  sink.record_latency(1, std::chrono::nanoseconds{200});

  auto samples = sink.drain();
  assert(samples.size() == 2);

  auto latency = sink.drain_latency();
  assert(!latency.empty());
  assert(latency.front().count == 2);
}

}  // namespace tradecore::tests
