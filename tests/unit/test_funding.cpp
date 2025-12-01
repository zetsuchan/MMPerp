#include "test_funding.hpp"

#include <cassert>
#include "tradecore/funding/funding_engine.hpp"

namespace tradecore::tests {

void test_funding_engine() {
  funding::FundingEngine funding;
  funding.configure_market(1, {.clamp_basis_points = 50, .max_rate_basis_points = 100});

  const auto snapshot = funding.update_market(1, 1'000, 1'020, 1);
  assert(snapshot.mark_price == 1'005);
  assert(snapshot.premium_rate == 50);
  assert(funding.accumulated_funding(1) == 50);
}

}  // namespace tradecore::tests
