#pragma once

#include <cstdint>
#include <vector>

#include "tradecore/common/types.hpp"
#include "tradecore/funding/funding_engine.hpp"
#include "tradecore/risk/risk_engine.hpp"

namespace tradecore {
namespace funding {

class FundingApplicator {
 public:
  struct FundingPayment {
    common::AccountId account{};
    common::MarketId market{};
    std::int64_t payment{0};
    std::int64_t funding_rate{0};
  };

  explicit FundingApplicator(FundingEngine& funding_engine, risk::RiskEngine& risk_engine)
      : funding_engine_(funding_engine), risk_engine_(risk_engine) {}

  std::vector<FundingPayment> apply_funding(const std::vector<common::MarketId>& markets);

 private:
  FundingEngine& funding_engine_;
  risk::RiskEngine& risk_engine_;
};

}  // namespace funding
}  // namespace tradecore
