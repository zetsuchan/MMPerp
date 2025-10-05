#include "tradecore/funding/funding_engine.hpp"

namespace tradecore {
namespace funding {

void FundingEngine::configure(std::int64_t clamp_bps) {
  clamp_basis_points_ = clamp_bps;
}

FundingSnapshot FundingEngine::compute_snapshot(common::MarketId /*market*/) const {
  FundingSnapshot snapshot;
  snapshot.mark_price = 0;
  snapshot.index_price = 0;
  snapshot.premium_rate = clamp_basis_points_;
  return snapshot;
}

}  // namespace funding
}  // namespace tradecore
