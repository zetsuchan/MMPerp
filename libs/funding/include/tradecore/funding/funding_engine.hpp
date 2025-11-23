#pragma once

#include <cstdint>
#include <memory_resource>
#include <unordered_map>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace funding {

struct FundingSnapshot {
  std::int64_t mark_price{0};
  std::int64_t index_price{0};
  std::int64_t premium_rate{0};      // basis points
  std::int64_t funding_rate{0};      // basis points
};

struct MarketFundingConfig {
  std::int32_t clamp_basis_points{100};
  std::int64_t max_rate_basis_points{200};
};

class FundingEngine {
 public:
  FundingEngine();

  void configure_market(common::MarketId market, MarketFundingConfig config);
  [[nodiscard]] FundingSnapshot update_market(common::MarketId market,
                                              std::int64_t index_price,
                                              std::int64_t mid_price,
                                              std::int64_t elapsed_seconds);

  [[nodiscard]] std::int64_t mark_price(common::MarketId market) const;
  [[nodiscard]] std::int64_t accumulated_funding(common::MarketId market) const;
  void reset_accumulated_funding(common::MarketId market);

 private:
  struct MarketState {
    MarketFundingConfig config{};
    std::int64_t mark_price{0};
    std::int64_t index_price{0};
    std::int64_t premium_rate{0};
    std::int64_t funding_accumulator{0};
  };

  std::pmr::monotonic_buffer_resource arena_;
  std::pmr::unordered_map<common::MarketId, MarketState> markets_;

  MarketState& ensure_market(common::MarketId market);
  static std::int64_t clamp(std::int64_t value, std::int64_t min_value, std::int64_t max_value);
};

}  // namespace funding
}  // namespace tradecore
