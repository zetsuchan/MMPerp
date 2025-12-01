#include "test_risk.hpp"

#include <cassert>
#include "tradecore/risk/liquidation_engine.hpp"
#include "tradecore/risk/risk_engine.hpp"

namespace tradecore::tests {

void test_risk_engine() {
  risk::RiskEngine risk;
  risk.configure_market(1, {.contract_size = 1,
                            .initial_margin_basis_points = 500,
                            .maintenance_margin_basis_points = 300});
  risk.set_mark_price(1, 1'000);
  risk.credit_collateral(1'001, 30'000);

  risk::OrderIntent open_intent{
      .account = 1'001,
      .market = 1,
      .side = common::Side::kBuy,
      .quantity = 400,
      .limit_price = 1'000,
      .reduce_only = false,
  };
  auto open_eval = risk.evaluate_order(open_intent);
  assert(open_eval.decision == risk::Decision::kAccepted);

  risk.apply_fill({.account = 1'001,
                   .market = 1,
                   .side = common::Side::kBuy,
                   .quantity = 400,
                   .price = 1'000});

  // Test reduce-only rejection on same-side order during drawdown
  risk.set_mark_price(1, 960);
  auto reduce_eval = risk.evaluate_order({
      .account = 1'001,
      .market = 1,
      .side = common::Side::kBuy,
      .quantity = 10,
      .limit_price = 950,
      .reduce_only = true,
  });
  assert(reduce_eval.decision == risk::Decision::kRejectedReduceOnly);
}

void test_liquidation() {
  risk::RiskEngine risk;
  risk.configure_market(1, {.contract_size = 1,
                            .initial_margin_basis_points = 500,
                            .maintenance_margin_basis_points = 300});
  risk.set_mark_price(1, 1'000);
  risk.credit_collateral(1'001, 30'000);

  risk.apply_fill({.account = 1'001,
                   .market = 1,
                   .side = common::Side::kBuy,
                   .quantity = 400,
                   .price = 1'000});

  risk::LiquidationManager liquidation{risk};

  // Partial liquidation at moderate drawdown
  risk.set_mark_price(1, 960);
  auto partial_liq = liquidation.evaluate(1'001);
  assert(partial_liq.status == risk::LiquidationManager::Status::kNeedsPartial);

  // Full liquidation at severe drawdown
  risk.set_mark_price(1, 900);
  auto full_liq = liquidation.evaluate(1'001);
  assert(full_liq.status == risk::LiquidationManager::Status::kNeedsFull);
}

}  // namespace tradecore::tests
