#include "tradecore/risk/liquidation_engine.hpp"

namespace tradecore {
namespace risk {

LiquidationManager::Result LiquidationManager::evaluate(common::AccountId account) const {
  const MarginSummary summary = engine_.account_summary(account);
  Result result;
  result.equity = summary.equity;
  result.initial_margin = summary.initial_margin;
  result.maintenance_margin = summary.maintenance_margin;

  if (summary.maintenance_margin == 0) {
    result.status = Status::kHealthy;
    return result;
  }

  if (summary.equity < summary.maintenance_margin) {
    result.status = Status::kNeedsFull;
    result.deficit = summary.maintenance_margin - summary.equity;
    return result;
  }

  if (summary.equity < summary.initial_margin) {
    result.status = Status::kNeedsPartial;
    result.deficit = summary.initial_margin - summary.equity;
    return result;
  }

  result.status = Status::kHealthy;
  return result;
}

}  // namespace risk
}  // namespace tradecore
