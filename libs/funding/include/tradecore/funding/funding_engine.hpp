#pragma once

#include <cstdint>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace funding {

struct FundingSnapshot {
  std::int64_t mark_price{};
  std::int64_t index_price{};
  std::int64_t premium_rate{};
};

class FundingEngine {
 public:
  void configure(std::int64_t clamp_bps);
  [[nodiscard]] FundingSnapshot compute_snapshot(common::MarketId market) const;

 private:
  std::int64_t clamp_basis_points_{100};
};

}  // namespace funding
}  // namespace tradecore
