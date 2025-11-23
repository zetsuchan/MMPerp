#include "tradecore/funding/funding_applicator.hpp"

namespace tradecore {
namespace funding {

std::vector<FundingApplicator::FundingPayment> FundingApplicator::apply_funding(
    const std::vector<common::MarketId>& markets) {
  std::vector<FundingPayment> payments;

  for (const auto& market_id : markets) {
    const std::int64_t accumulated_funding = funding_engine_.accumulated_funding(market_id);
    
    if (accumulated_funding == 0) {
      continue;
    }

    const auto all_accounts = risk_engine_.get_all_accounts();
    
    for (const auto& account_id : all_accounts) {
      const auto* account_state = risk_engine_.find_account(account_id);
      if (!account_state) {
        continue;
      }

      auto pos_it = account_state->positions.find(market_id);
      if (pos_it == account_state->positions.end() || pos_it->second.quantity == 0) {
        continue;
      }

      const std::int64_t position_qty = pos_it->second.quantity;
      const auto* market_state = risk_engine_.find_market(market_id);
      if (!market_state) {
        continue;
      }

      const std::int64_t contract_size = market_state->config.contract_size;
      const std::int64_t payment = (position_qty * accumulated_funding * contract_size) / 10000;

      risk_engine_.credit_collateral(account_id, -payment);

      payments.push_back(FundingPayment{
          .account = account_id,
          .market = market_id,
          .payment = payment,
          .funding_rate = accumulated_funding
      });
    }

    funding_engine_.reset_accumulated_funding(market_id);
  }

  return payments;
}

}  // namespace funding
}  // namespace tradecore
