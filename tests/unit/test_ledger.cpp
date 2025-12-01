#include "test_ledger.hpp"

#include <cassert>
#include "tradecore/ledger/ledger_state.hpp"

namespace tradecore::tests {

void test_ledger_credit_debit() {
  ledger::LedgerState ledger;
  ledger.credit(7, 100);
  ledger.debit(7, 10);
  const auto account = ledger.get(7);
  assert(account.collateral_available == 90);
  assert(account.collateral_locked == 10);
}

}  // namespace tradecore::tests
