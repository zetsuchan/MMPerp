#include "tradecore/risk/liquidation_executor.hpp"

#include <cstdlib>

namespace tradecore {
namespace risk {

std::vector<LiquidationExecutor::LiquidationOrder> LiquidationExecutor::check_and_liquidate_accounts(
    const std::vector<common::AccountId>& accounts) {
  std::vector<LiquidationOrder> liquidation_orders;
  LiquidationManager liquidation_manager(risk_engine_);

  for (const auto& account : accounts) {
    auto result = liquidation_manager.evaluate(account);
    
    if (result.status == LiquidationManager::Status::kHealthy) {
      continue;
    }

    const auto* account_state = risk_engine_.find_account(account);
    if (!account_state) {
      continue;
    }

    for (const auto& [market_id, position] : account_state->positions) {
      if (position.quantity == 0) {
        continue;
      }

      LiquidationOrder order;
      order.account = account;
      order.market = market_id;
      order.side = (position.quantity > 0) ? common::Side::kSell : common::Side::kBuy;
      order.quantity = std::llabs(position.quantity);

      liquidation_orders.push_back(order);
      execute_liquidation(order);
    }
  }

  return liquidation_orders;
}

void LiquidationExecutor::execute_liquidation(const LiquidationOrder& order) {
  common::OrderId liquidation_order_id{
      .market = order.market,
      .session = 0,
      .local = next_liquidation_order_id_++
  };

  matcher::OrderRequest request{
      .id = liquidation_order_id,
      .account = order.account,
      .side = order.side,
      .quantity = order.quantity,
      .price = (order.side == common::Side::kBuy) ? std::numeric_limits<std::int64_t>::max() 
                                                    : std::numeric_limits<std::int64_t>::min(),
      .tif = common::TimeInForce::kIoc,
      .flags = static_cast<std::uint16_t>(common::OrderFlags::kReduceOnly)
  };

  auto result = matching_engine_.submit(request);

  if (result.accepted && !result.fills.empty()) {
    for (const auto& fill : result.fills) {
      const bool is_maker = (fill.maker_order == liquidation_order_id);
      const auto fill_side = is_maker ? order.side : 
                             (order.side == common::Side::kBuy ? common::Side::kSell : common::Side::kBuy);
      
      risk::FillContext fill_context{
          .account = order.account,
          .market = order.market,
          .side = fill_side,
          .quantity = fill.quantity,
          .price = fill.price
      };
      
      risk_engine_.apply_fill(fill_context);
    }
  }
}

}  // namespace risk
}  // namespace tradecore
