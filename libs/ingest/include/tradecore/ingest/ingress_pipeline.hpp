#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "tradecore/common/time_utils.hpp"
#include "tradecore/common/types.hpp"

namespace tradecore {
namespace ingest {

struct FrameView {
  std::span<const std::byte> payload;
  common::TimestampNs received_time_ns{};
};

class IngressPipeline {
 public:
  void configure(std::string_view endpoint_uri);
  [[nodiscard]] bool enqueue(FrameView frame);
  void drain();

 private:
  std::string endpoint_{};
};

}  // namespace ingest
}  // namespace tradecore
