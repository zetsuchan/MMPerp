#pragma once

#include <cstdint>
#include <vector>

#include "tradecore/common/types.hpp"
#include "tradecore/matcher/matching_engine.hpp"
#include "tradecore/risk/liquidation_engine.hpp"
#include "tradecore/risk/risk_engine.hpp"

namespace tradecore {
namespace risk {

class LiquidationExecutor {
 public:
  struct LiquidationOrder {
    common::AccountId account{};
    common::MarketId market{};
    common::Side side{common::Side::kBuy};
    std::int64_t quantity{0};
  };

  explicit LiquidationExecutor(RiskEngine& risk_engine, matcher::MatchingEngine& matching_engine)
      : risk_engine_(risk_engine), matching_engine_(matching_engine) {}

  std::vector<LiquidationOrder> check_and_liquidate_accounts(const std::vector<common::AccountId>& accounts);

  void execute_liquidation(const LiquidationOrder& order);

 private:
  RiskEngine& risk_engine_;
  matcher::MatchingEngine& matching_engine_;
  std::uint64_t next_liquidation_order_id_{1};
};

}  // namespace risk
}  // namespace tradecore
