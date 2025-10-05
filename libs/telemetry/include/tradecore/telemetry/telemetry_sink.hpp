#pragma once

#include <cstdint>
#include <vector>

namespace tradecore {
namespace telemetry {

struct Sample {
  std::uint64_t id{};
  std::int64_t value{};
};

class TelemetrySink {
 public:
  void push(Sample sample);
  [[nodiscard]] std::vector<Sample> drain();

 private:
  std::vector<Sample> buffer_{};
};

}  // namespace telemetry
}  // namespace tradecore
