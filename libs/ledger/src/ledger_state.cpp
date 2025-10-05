#include "tradecore/ledger/ledger_state.hpp"

namespace tradecore {
namespace ledger {

void LedgerState::credit(common::SessionId session, std::int64_t amount) {
  accounts_[session].collateral_available += amount;
}

void LedgerState::debit(common::SessionId session, std::int64_t amount) {
  accounts_[session].collateral_available -= amount;
  accounts_[session].collateral_locked += amount;
}

AccountState LedgerState::get(common::SessionId session) const {
  if (auto it = accounts_.find(session); it != accounts_.end()) {
    return it->second;
  }
  return {};
}

}  // namespace ledger
}  // namespace tradecore
