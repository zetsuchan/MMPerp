#pragma once

#include <cstdint>

#include "tradecore/common/types.hpp"
#include "tradecore/risk/risk_engine.hpp"

namespace tradecore {
namespace risk {

class LiquidationManager {
 public:
  enum class Status : std::uint8_t {
    kHealthy,
    kNeedsPartial,
    kNeedsFull,
  };

  struct Result {
    Status status{Status::kHealthy};
    std::int64_t equity{0};
    std::int64_t initial_margin{0};
    std::int64_t maintenance_margin{0};
    std::int64_t deficit{0};
  };

  explicit LiquidationManager(const RiskEngine& engine) : engine_(engine) {}

  [[nodiscard]] Result evaluate(common::AccountId account) const;

 private:
  const RiskEngine& engine_;
};

}  // namespace risk
}  // namespace tradecore
