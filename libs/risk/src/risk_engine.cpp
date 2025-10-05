#include "tradecore/risk/risk_engine.hpp"

namespace tradecore {
namespace risk {

void RiskEngine::bootstrap() {
  // Placeholder: load risk parameters, margin tiers, and account state caches.
}

RiskResult RiskEngine::evaluate_order(common::OrderId /*order_id*/, std::int64_t notional) const {
  RiskResult result;
  result.margin_requirement = notional;  // Placeholder: identity mapping for now.
  return result;
}

}  // namespace risk
}  // namespace tradecore
