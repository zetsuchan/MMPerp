#include "tradecore/risk/risk_engine.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace tradecore {
namespace risk {

namespace {
constexpr std::int64_t kBasisPointDenominator = 10'000;
constexpr std::uint16_t kRejectCodeUnknownMarket = 2001;
constexpr std::uint16_t kRejectCodeInsufficientMargin = 2002;
constexpr std::uint16_t kRejectCodeReduceOnly = 2003;
}  // namespace

RiskEngine::RiskEngine(std::size_t arena_bytes)
    : arena_(arena_bytes),
      accounts_(&arena_),
      markets_(&arena_) {}

void RiskEngine::configure_market(common::MarketId market, MarketRiskConfig config) {
  auto [it, inserted] = markets_.try_emplace(market, MarketState{.config = config, .mark_price = 0});
  if (!inserted) {
    it->second.config = config;
  }
}

void RiskEngine::set_mark_price(common::MarketId market, std::int64_t mark_price) {
  ensure_market(market).mark_price = mark_price;
}

void RiskEngine::credit_collateral(common::AccountId account, std::int64_t amount) {
  ensure_account(account).collateral += amount;
}

void RiskEngine::debit_collateral(common::AccountId account, std::int64_t amount) {
  ensure_account(account).collateral -= amount;
}

void RiskEngine::apply_fill(const FillContext& fill) {
  auto& market = ensure_market(fill.market);
  auto& account = ensure_account(fill.account);
  auto& position = account.positions.try_emplace(fill.market, PositionState{}).first->second;

  const std::int64_t signed_qty = (fill.side == common::Side::kBuy) ? fill.quantity : -fill.quantity;
  const std::int64_t contract_size = market.config.contract_size;
  const std::int64_t previous_qty = position.quantity;

  if (previous_qty == 0 || (previous_qty > 0 && signed_qty > 0) || (previous_qty < 0 && signed_qty < 0)) {
    const std::int64_t new_qty = previous_qty + signed_qty;
    if (new_qty != 0) {
      const std::int64_t total_abs = std::llabs(previous_qty) + std::llabs(signed_qty);
      const std::int64_t weighted_notional = position.entry_price * std::llabs(previous_qty) + fill.price * std::llabs(signed_qty);
      position.entry_price = total_abs == 0 ? 0 : weighted_notional / total_abs;
    } else {
      position.entry_price = 0;
    }
    position.quantity = new_qty;
    return;
  }

  const std::int64_t closing_qty = std::min<std::int64_t>(std::llabs(previous_qty), std::llabs(signed_qty));
  const std::int64_t pnl_per_contract = (previous_qty > 0)
                                            ? (fill.price - position.entry_price)
                                            : (position.entry_price - fill.price);
  const std::int64_t realized = closing_qty * pnl_per_contract * contract_size;
  account.realized_pnl += realized;
  account.collateral += realized;

  const std::int64_t remainder = previous_qty + signed_qty;
  position.quantity = remainder;
  if (remainder == 0) {
    position.entry_price = 0;
  } else if ((previous_qty > 0 && remainder < 0) || (previous_qty < 0 && remainder > 0)) {
    position.entry_price = fill.price;
  }
}

RiskResult RiskEngine::evaluate_order(const OrderIntent& intent) const {
  RiskResult result;

  const auto* market = find_market(intent.market);
  if (!market) {
    result.decision = Decision::kRejectedUnknownMarket;
    result.reject_code = kRejectCodeUnknownMarket;
    return result;
  }

  const auto* account = find_account(intent.account);
  std::int64_t existing_qty = 0;
  if (account) {
    if (auto it = account->positions.find(intent.market); it != account->positions.end()) {
      existing_qty = it->second.quantity;
    }
  }

  const std::int64_t signed_qty = (intent.side == common::Side::kBuy) ? intent.quantity : -intent.quantity;
  const std::int64_t projected_qty = existing_qty + signed_qty;

  if (intent.reduce_only && std::llabs(projected_qty) > std::llabs(existing_qty)) {
    result.decision = Decision::kRejectedReduceOnly;
    result.reject_code = kRejectCodeReduceOnly;
    return result;
  }

  const MarginSummary summary = account_summary_with_delta(intent.account, FillContext{
                                                           .account = intent.account,
                                                           .market = intent.market,
                                                           .side = intent.side,
                                                           .quantity = intent.quantity,
                                                           .price = intent.limit_price,
                                                       });
  result.equity = summary.equity;
  result.initial_margin_required = summary.initial_margin;
  result.maintenance_margin_required = summary.maintenance_margin;

  if (summary.initial_margin > summary.equity) {
    result.decision = Decision::kRejectedInsufficientMargin;
    result.reject_code = kRejectCodeInsufficientMargin;
    return result;
  }

  result.decision = Decision::kAccepted;
  return result;
}

MarginSummary RiskEngine::account_summary(common::AccountId account) const {
  return account_summary_with_delta(account, std::nullopt);
}

RiskEngine::AccountState& RiskEngine::ensure_account(common::AccountId account) {
  auto it = accounts_.find(account);
  if (it == accounts_.end()) {
    it = accounts_.emplace(account, AccountState{&arena_}).first;
  }
  return it->second;
}

const RiskEngine::AccountState* RiskEngine::find_account(common::AccountId account) const {
  auto it = accounts_.find(account);
  if (it == accounts_.end()) {
    return nullptr;
  }
  return &it->second;
}

RiskEngine::MarketState& RiskEngine::ensure_market(common::MarketId market) {
  auto it = markets_.find(market);
  if (it == markets_.end()) {
    it = markets_.emplace(market, MarketState{}).first;
  }
  return it->second;
}

const RiskEngine::MarketState* RiskEngine::find_market(common::MarketId market) const {
  auto it = markets_.find(market);
  if (it == markets_.end()) {
    return nullptr;
  }
  return &it->second;
}

MarginSummary RiskEngine::account_summary_with_delta(common::AccountId account,
                                                      std::optional<FillContext> delta) const {
  MarginSummary summary{};
  const auto* account_state = find_account(account);
  if (account_state) {
    summary.equity = account_state->collateral + account_state->realized_pnl;
  }

  struct Exposure {
    common::MarketId market;
    std::int64_t quantity;
    std::int64_t entry_price;
    bool existed;
  };

  std::vector<Exposure> exposures;
  exposures.reserve(account_state ? account_state->positions.size() + 1 : 1);

  if (account_state) {
    for (const auto& [market_id, position] : account_state->positions) {
      exposures.push_back(Exposure{market_id, position.quantity, position.entry_price, true});
    }
  }

  if (delta.has_value()) {
    const std::int64_t signed_qty = (delta->side == common::Side::kBuy) ? delta->quantity : -delta->quantity;
    auto it = std::find_if(exposures.begin(), exposures.end(), [&](const Exposure& e) {
      return e.market == delta->market;
    });
    if (it != exposures.end()) {
      it->quantity += signed_qty;
      if (!it->existed) {
        it->entry_price = delta->price;
      }
    } else {
      exposures.push_back(Exposure{delta->market, signed_qty, delta->price, false});
    }
  }

  for (const auto& exposure : exposures) {
    if (exposure.quantity == 0) {
      continue;
    }
    const auto* market_state = find_market(exposure.market);
    if (!market_state) {
      continue;
    }

    std::int64_t mark_price = market_state->mark_price;
    if (mark_price == 0) {
      if (delta.has_value() && delta->market == exposure.market && delta->price != 0) {
        mark_price = delta->price;
      } else if (exposure.entry_price != 0) {
        mark_price = exposure.entry_price;
      }
    }
    const std::int64_t contract_size = market_state->config.contract_size;
    const std::int64_t notional = std::llabs(exposure.quantity) * mark_price * contract_size;

    summary.initial_margin += apply_basis_points(notional, market_state->config.initial_margin_basis_points);
    summary.maintenance_margin += apply_basis_points(notional, market_state->config.maintenance_margin_basis_points);

    if (account_state && exposure.existed) {
      const std::int64_t unrealized = exposure.quantity * (mark_price - exposure.entry_price) * contract_size;
      summary.equity += unrealized;
    }
  }

  return summary;
}

std::int64_t RiskEngine::apply_basis_points(std::int64_t notional, std::int32_t basis_points) {
  return (notional * basis_points + (kBasisPointDenominator - 1)) / kBasisPointDenominator;
}

}  // namespace risk
}  // namespace tradecore
