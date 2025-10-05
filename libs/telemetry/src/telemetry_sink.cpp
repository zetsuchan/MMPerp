#include "tradecore/telemetry/telemetry_sink.hpp"

namespace tradecore {
namespace telemetry {

void TelemetrySink::push(Sample sample) {
  buffer_.push_back(sample);
}

std::vector<Sample> TelemetrySink::drain() {
  auto copy = buffer_;
  buffer_.clear();
  return copy;
}

}  // namespace telemetry
}  // namespace tradecore
