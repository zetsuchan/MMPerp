#pragma once

#include <cstdint>
#include <memory_resource>
#include <optional>
#include <unordered_map>
#include <vector>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace risk {

enum class Decision : std::uint8_t {
  kAccepted,
  kRejectedInsufficientMargin,
  kRejectedReduceOnly,
  kRejectedUnknownMarket,
};

struct RiskResult {
  Decision decision{Decision::kAccepted};
  std::uint16_t reject_code{0};
  std::int64_t equity{0};
  std::int64_t initial_margin_required{0};
  std::int64_t maintenance_margin_required{0};
};

struct MarketRiskConfig {
  std::int64_t contract_size{1};             // notional per contract in quote units
  std::int32_t initial_margin_basis_points{0};
  std::int32_t maintenance_margin_basis_points{0};
};

struct OrderIntent {
  common::AccountId account{0};
  common::MarketId market{0};
  common::Side side{common::Side::kBuy};
  std::int64_t quantity{0};
  std::int64_t limit_price{0};
  bool reduce_only{false};
};

struct FillContext {
  common::AccountId account{0};
  common::MarketId market{0};
  common::Side side{common::Side::kBuy};
  std::int64_t quantity{0};
  std::int64_t price{0};
};

struct MarginSummary {
  std::int64_t equity{0};
  std::int64_t initial_margin{0};
  std::int64_t maintenance_margin{0};
};

class RiskEngine {
 public:
  explicit RiskEngine(std::size_t arena_bytes = 1 << 20);

  void configure_market(common::MarketId market, MarketRiskConfig config);
  void set_mark_price(common::MarketId market, std::int64_t mark_price);

  void credit_collateral(common::AccountId account, std::int64_t amount);
  void debit_collateral(common::AccountId account, std::int64_t amount);

  void apply_fill(const FillContext& fill);
  [[nodiscard]] RiskResult evaluate_order(const OrderIntent& intent) const;
  [[nodiscard]] MarginSummary account_summary(common::AccountId account) const;

 private:
  struct PositionState {
    std::int64_t quantity{0};
    std::int64_t entry_price{0};
  };

  struct AccountState {
    std::int64_t collateral{0};
    std::int64_t realized_pnl{0};
    std::pmr::unordered_map<common::MarketId, PositionState> positions;

    explicit AccountState(std::pmr::memory_resource* mem)
        : positions(mem) {}
  };

  struct MarketState {
    MarketRiskConfig config{};
    std::int64_t mark_price{0};
  };

  mutable std::pmr::monotonic_buffer_resource arena_;
  std::pmr::unordered_map<common::AccountId, AccountState> accounts_;
  std::pmr::unordered_map<common::MarketId, MarketState> markets_;

  AccountState& ensure_account(common::AccountId account);
  const AccountState* find_account(common::AccountId account) const;
  MarketState& ensure_market(common::MarketId market);
  const MarketState* find_market(common::MarketId market) const;

  [[nodiscard]] MarginSummary account_summary_with_delta(common::AccountId account,
                                                         std::optional<FillContext> delta) const;
  static std::int64_t apply_basis_points(std::int64_t notional, std::int32_t basis_points);
};

}  // namespace risk
}  // namespace tradecore
