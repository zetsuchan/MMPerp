#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory_resource>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace matcher {

struct FillEvent {
  common::OrderId maker_order;
  common::OrderId taker_order;
  std::int64_t quantity{};
  std::int64_t price{};
};

struct OrderRequest {
  common::OrderId id{};
  common::AccountId account{};
  common::Side side{common::Side::kBuy};
  std::int64_t quantity{0};
  std::int64_t price{0};
  common::TimeInForce tif{common::TimeInForce::kGtc};
  std::uint16_t flags{common::kFlagsNone};
};

struct CancelRequest {
  common::OrderId id{};
};

struct ReplaceRequest {
  common::OrderId id{};
  std::int64_t new_quantity{0};
  std::int64_t new_price{0};
  common::TimeInForce new_tif{common::TimeInForce::kGtc};
  std::uint16_t new_flags{common::kFlagsNone};
};

struct OrderResult {
  bool accepted{false};
  bool fully_filled{false};
  bool resting{false};
  std::uint16_t reject_code{0};
  std::vector<FillEvent> fills{};
};

struct CancelResult {
  bool cancelled{false};
  std::uint16_t reject_code{0};
};

struct ReplaceResult {
  bool accepted{false};
  bool resting{false};
  std::uint16_t reject_code{0};
  std::vector<FillEvent> fills{};
};

class MatchingEngine {
 public:
  struct Config {
    std::size_t arena_bytes{1 << 20};
  };

  explicit MatchingEngine(const Config& config = Config{});

  void add_market(common::MarketId market_id);
  void clear_market(common::MarketId market_id);

  [[nodiscard]] OrderResult submit(const OrderRequest& request);
  [[nodiscard]] CancelResult cancel(const CancelRequest& request);
  [[nodiscard]] ReplaceResult replace(const ReplaceRequest& request);

 private:
  struct OrderRecord;
  struct PriceLevel;

  struct MarketShard {
    std::pmr::unordered_map<std::uint64_t, OrderRecord> book_orders;
    std::pmr::map<std::int64_t, PriceLevel, std::greater<>> bids;
    std::pmr::map<std::int64_t, PriceLevel, std::less<>> asks;
    std::uint64_t next_sequence{1};

    explicit MarketShard(std::pmr::memory_resource* mem);
  };

  struct OrderRecord {
    OrderRequest request;
    std::int64_t remaining{0};
    PriceLevel* level{nullptr};
    OrderRecord* prev{nullptr};
    OrderRecord* next{nullptr};
    std::uint64_t fifo_seq{0};
  };

  struct PriceLevel {
    OrderRecord* head{nullptr};
    OrderRecord* tail{nullptr};
    std::int64_t total_qty{0};

    void push_back(OrderRecord* record);
    void remove(OrderRecord* record);
    bool empty() const noexcept { return head == nullptr; }
  };

  using MarketMap = std::pmr::unordered_map<common::MarketId, MarketShard>;

  std::pmr::monotonic_buffer_resource arena_;
  MarketMap markets_;

  MarketShard& ensure_market(common::MarketId market_id);
  [[nodiscard]] static std::uint64_t encode_order_id(const common::OrderId& id) noexcept;
  [[nodiscard]] static bool crosses(common::Side side, std::int64_t taker_price, std::int64_t maker_price) noexcept;
  [[nodiscard]] std::int64_t fillable_quantity(const MarketShard& shard, const OrderRequest& req) const;
  [[nodiscard]] OrderResult place_order(MarketShard& shard, OrderRequest order);
  void match_order(MarketShard& shard, OrderRecord& taker_record, std::vector<FillEvent>& fills);
  void rest_order(MarketShard& shard, OrderRecord& record);
  void remove_order_from_book(MarketShard& shard, OrderRecord& record);
};

}  // namespace matcher
}  // namespace tradecore
