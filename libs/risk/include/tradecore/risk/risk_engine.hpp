#pragma once

#include <cstdint>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace risk {

enum class Decision : std::uint8_t {
  kAccepted,
  kRejectedInsufficientMargin,
  kRejectedRateLimit,
};

struct RiskResult {
  Decision decision{Decision::kAccepted};
  std::int64_t margin_requirement{};
};

class RiskEngine {
 public:
  void bootstrap();
  [[nodiscard]] RiskResult evaluate_order(common::OrderId order_id, std::int64_t notional) const;
};

}  // namespace risk
}  // namespace tradecore
