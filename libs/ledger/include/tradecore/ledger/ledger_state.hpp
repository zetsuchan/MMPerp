#pragma once

#include <cstdint>
#include <unordered_map>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace ledger {

struct AccountState {
  std::int64_t collateral_available{};
  std::int64_t collateral_locked{};
};

class LedgerState {
 public:
  void credit(common::SessionId session, std::int64_t amount);
  void debit(common::SessionId session, std::int64_t amount);
  [[nodiscard]] AccountState get(common::SessionId session) const;

 private:
  std::unordered_map<common::SessionId, AccountState> accounts_{};
};

}  // namespace ledger
}  // namespace tradecore
