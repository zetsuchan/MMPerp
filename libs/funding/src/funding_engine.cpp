#include "tradecore/funding/funding_engine.hpp"

#include <algorithm>

namespace tradecore {
namespace funding {

namespace {
constexpr std::int64_t kBasisPointDenominator = 10'000;
}

FundingEngine::FundingEngine()
    : arena_(1 << 16),
      markets_(&arena_) {}

void FundingEngine::configure_market(common::MarketId market, MarketFundingConfig config) {
  auto [it, inserted] = markets_.try_emplace(market, MarketState{.config = config});
  if (!inserted) {
    it->second.config = config;
  }
}

FundingSnapshot FundingEngine::update_market(common::MarketId market,
                                              std::int64_t index_price,
                                              std::int64_t mid_price,
                                              std::int64_t elapsed_seconds) {
  auto& state = ensure_market(market);
  state.index_price = index_price;

  const std::int64_t band = (index_price * state.config.clamp_basis_points) / kBasisPointDenominator;
  const std::int64_t clamped_mark = clamp(mid_price, index_price - band, index_price + band);
  state.mark_price = clamped_mark;

  std::int64_t premium = 0;
  if (index_price > 0) {
    premium = ((mid_price - index_price) * kBasisPointDenominator) / index_price;
  }
  premium = clamp(premium, -state.config.clamp_basis_points, state.config.clamp_basis_points);
  state.premium_rate = premium;

  const std::int64_t funding_rate = clamp(premium, -state.config.max_rate_basis_points, state.config.max_rate_basis_points);
  state.funding_accumulator += funding_rate * elapsed_seconds;

  return FundingSnapshot{
      .mark_price = state.mark_price,
      .index_price = state.index_price,
      .premium_rate = state.premium_rate,
      .funding_rate = funding_rate,
  };
}

std::int64_t FundingEngine::mark_price(common::MarketId market) const {
  auto it = markets_.find(market);
  if (it == markets_.end()) {
    return 0;
  }
  return it->second.mark_price;
}

std::int64_t FundingEngine::accumulated_funding(common::MarketId market) const {
  auto it = markets_.find(market);
  if (it == markets_.end()) {
    return 0;
  }
  return it->second.funding_accumulator;
}

FundingEngine::MarketState& FundingEngine::ensure_market(common::MarketId market) {
  auto it = markets_.find(market);
  if (it == markets_.end()) {
    it = markets_.emplace(market, MarketState{}).first;
  }
  return it->second;
}

std::int64_t FundingEngine::clamp(std::int64_t value, std::int64_t min_value, std::int64_t max_value) {
  return std::min(std::max(value, min_value), max_value);
}

void FundingEngine::reset_accumulated_funding(common::MarketId market) {
  auto it = markets_.find(market);
  if (it != markets_.end()) {
    it->second.funding_accumulator = 0;
  }
}

}  // namespace funding
}  // namespace tradecore
