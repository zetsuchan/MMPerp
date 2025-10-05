#include "tradecore/matcher/matching_engine.hpp"

#include <algorithm>

namespace tradecore {
namespace matcher {

namespace {
constexpr std::uint16_t kRejectUnknownMarket = 1001;
constexpr std::uint16_t kRejectInsufficientLiquidity = 1002;
constexpr std::uint16_t kRejectPostOnlyWouldCross = 1003;
constexpr std::uint16_t kRejectOrderNotFound = 1004;
constexpr std::uint16_t kRejectInvalidQuantity = 1005;
constexpr std::uint16_t kRejectDuplicateOrderId = 1006;
}  // namespace

MatchingEngine::MarketShard::MarketShard(std::pmr::memory_resource* mem)
    : book_orders(mem),
      bids(std::greater<>{},
           std::pmr::polymorphic_allocator<std::pair<const std::int64_t, PriceLevel>>(mem)),
      asks(std::less<>{},
           std::pmr::polymorphic_allocator<std::pair<const std::int64_t, PriceLevel>>(mem)) {}

void MatchingEngine::PriceLevel::push_back(OrderRecord* record) {
  record->prev = tail;
  record->next = nullptr;
  if (tail) {
    tail->next = record;
  } else {
    head = record;
  }
  tail = record;
  record->level = this;
  total_qty += record->remaining;
}

void MatchingEngine::PriceLevel::remove(OrderRecord* record) {
  total_qty -= record->remaining;
  if (record->prev) {
    record->prev->next = record->next;
  } else {
    head = record->next;
  }
  if (record->next) {
    record->next->prev = record->prev;
  } else {
    tail = record->prev;
  }
  record->prev = nullptr;
  record->next = nullptr;
  record->level = nullptr;
}

MatchingEngine::MatchingEngine(const Config& config)
    : arena_(config.arena_bytes),
      markets_(&arena_) {}

void MatchingEngine::add_market(common::MarketId market_id) {
  markets_.try_emplace(market_id, &arena_);
}

void MatchingEngine::clear_market(common::MarketId market_id) {
  markets_.erase(market_id);
  markets_.emplace(market_id, MarketShard{&arena_});
}

MatchingEngine::MarketShard& MatchingEngine::ensure_market(common::MarketId market_id) {
  auto it = markets_.find(market_id);
  if (it == markets_.end()) {
    it = markets_.emplace(market_id, MarketShard{&arena_}).first;
  }
  return it->second;
}

std::uint64_t MatchingEngine::encode_order_id(const common::OrderId& id) noexcept {
  return id.value();
}

bool MatchingEngine::crosses(common::Side side, std::int64_t taker_price, std::int64_t maker_price) noexcept {
  if (side == common::Side::kBuy) {
    return maker_price <= taker_price;
  }
  return maker_price >= taker_price;
}

std::int64_t MatchingEngine::fillable_quantity(const MarketShard& shard, const OrderRequest& req) const {
  std::int64_t total{0};
  if (req.side == common::Side::kBuy) {
    for (const auto& [price, level] : shard.asks) {
      if (!crosses(req.side, req.price, price)) {
        break;
      }
      total += level.total_qty;
      if (total >= req.quantity) {
        return total;
      }
    }
  } else {
    for (const auto& [price, level] : shard.bids) {
      if (!crosses(req.side, req.price, price)) {
        break;
      }
      total += level.total_qty;
      if (total >= req.quantity) {
        return total;
      }
    }
  }
  return total;
}

OrderResult MatchingEngine::submit(const OrderRequest& request) {
  if (request.quantity <= 0) {
    return OrderResult{.reject_code = kRejectInvalidQuantity};
  }

  auto& shard = ensure_market(request.id.market);
  return place_order(shard, request);
}

CancelResult MatchingEngine::cancel(const CancelRequest& request) {
  auto it = markets_.find(request.id.market);
  if (it == markets_.end()) {
    return CancelResult{.cancelled = false, .reject_code = kRejectUnknownMarket};
  }

  auto& shard = it->second;
  const auto encoded = encode_order_id(request.id);
  auto ord_it = shard.book_orders.find(encoded);
  if (ord_it == shard.book_orders.end()) {
    return CancelResult{.cancelled = false, .reject_code = kRejectOrderNotFound};
  }

  remove_order_from_book(shard, ord_it->second);
  shard.book_orders.erase(ord_it);
  return CancelResult{.cancelled = true};
}

ReplaceResult MatchingEngine::replace(const ReplaceRequest& request) {
  auto it = markets_.find(request.id.market);
  if (it == markets_.end()) {
    return ReplaceResult{.reject_code = kRejectUnknownMarket};
  }

  auto& shard = it->second;
  const auto encoded = encode_order_id(request.id);
  auto ord_it = shard.book_orders.find(encoded);
  if (ord_it == shard.book_orders.end()) {
    return ReplaceResult{.reject_code = kRejectOrderNotFound};
  }

  // Preserve account/side, update price/qty/TIF/flags and reinsert with new FIFO sequence.
  OrderRecord record_copy = ord_it->second;
  remove_order_from_book(shard, ord_it->second);
  shard.book_orders.erase(ord_it);

  OrderRequest new_req = record_copy.request;
  new_req.price = request.new_price;
  new_req.quantity = request.new_quantity;
  new_req.tif = request.new_tif;
  new_req.flags = request.new_flags;
  new_req.id = request.id;

  auto result = place_order(shard, std::move(new_req));
  ReplaceResult replace_result;
  replace_result.accepted = result.accepted;
  replace_result.resting = result.resting;
  replace_result.reject_code = result.reject_code;
  replace_result.fills = std::move(result.fills);
  return replace_result;
}

OrderResult MatchingEngine::place_order(MarketShard& shard, OrderRequest order) {
  OrderResult result;
  const auto encoded = encode_order_id(order.id);

  if (common::HasFlag(order.flags, common::OrderFlags::kPostOnly)) {
    if (order.side == common::Side::kBuy) {
      if (!shard.asks.empty()) {
        const auto& [best_price, _] = *shard.asks.begin();
        if (crosses(order.side, order.price, best_price)) {
          result.reject_code = kRejectPostOnlyWouldCross;
          return result;
        }
      }
    } else {
      if (!shard.bids.empty()) {
        const auto& [best_price, _] = *shard.bids.begin();
        if (crosses(order.side, order.price, best_price)) {
          result.reject_code = kRejectPostOnlyWouldCross;
          return result;
        }
      }
    }
  }

  if (order.tif == common::TimeInForce::kFok) {
    const auto fillable = fillable_quantity(shard, order);
    if (fillable < order.quantity) {
      result.reject_code = kRejectInsufficientLiquidity;
      return result;
    }
  }

  OrderRecord taker_record;
  taker_record.request = order;
  taker_record.remaining = order.quantity;
  taker_record.fifo_seq = shard.next_sequence++;

  match_order(shard, taker_record, result.fills);

  if (taker_record.remaining > 0) {
    if (order.tif == common::TimeInForce::kIoc || order.tif == common::TimeInForce::kFok) {
      result.accepted = true;
      result.fully_filled = false;
      result.resting = false;
      return result;
    }

    auto [it, inserted] = shard.book_orders.emplace(encoded, std::move(taker_record));
    if (!inserted) {
      result.reject_code = kRejectDuplicateOrderId;
      return result;
    }
    rest_order(shard, it->second);
    result.resting = true;
  } else {
    result.fully_filled = true;
  }

  result.accepted = true;
  return result;
}

void MatchingEngine::match_order(MarketShard& shard, OrderRecord& taker_record, std::vector<FillEvent>& fills) {
  auto consume_book = [&](auto& book) {
    auto it = book.begin();
    while (taker_record.remaining > 0 && it != book.end()) {
      const auto maker_price = it->first;
      if (!crosses(taker_record.request.side, taker_record.request.price, maker_price)) {
        break;
      }

      auto& level = it->second;
      auto* maker = level.head;
      while (maker && taker_record.remaining > 0) {
        const auto traded = std::min(taker_record.remaining, maker->remaining);
        taker_record.remaining -= traded;
        maker->remaining -= traded;
        level.total_qty -= traded;

        fills.push_back(FillEvent{
            .maker_order = maker->request.id,
            .taker_order = taker_record.request.id,
            .quantity = traded,
            .price = maker_price,
        });

        if (maker->remaining == 0) {
          auto encoded = encode_order_id(maker->request.id);
          auto old = maker;
          maker = maker->next;
          level.remove(old);
          shard.book_orders.erase(encoded);
        } else {
          maker = maker->next;
        }
      }

      if (level.empty()) {
        it = book.erase(it);
      } else {
        ++it;
      }
    }
  };

  if (taker_record.request.side == common::Side::kBuy) {
    consume_book(shard.asks);
  } else {
    consume_book(shard.bids);
  }
}

void MatchingEngine::rest_order(MarketShard& shard, OrderRecord& record) {
  if (record.request.side == common::Side::kBuy) {
    auto [level_it, inserted] = shard.bids.try_emplace(record.request.price);
    auto& level = level_it->second;
    if (inserted) {
      level.head = nullptr;
      level.tail = nullptr;
      level.total_qty = 0;
    }
    level.push_back(&record);
  } else {
    auto [level_it, inserted] = shard.asks.try_emplace(record.request.price);
    auto& level = level_it->second;
    if (inserted) {
      level.head = nullptr;
      level.tail = nullptr;
      level.total_qty = 0;
    }
    level.push_back(&record);
  }
}

void MatchingEngine::remove_order_from_book(MarketShard& shard, OrderRecord& record) {
  if (!record.level) {
    return;
  }

  if (record.request.side == common::Side::kBuy) {
    auto it = shard.bids.find(record.request.price);
    if (it == shard.bids.end()) {
      return;
    }
    auto& level = it->second;
    level.remove(&record);
    if (level.empty()) {
      shard.bids.erase(it);
    }
  } else {
    auto it = shard.asks.find(record.request.price);
    if (it == shard.asks.end()) {
      return;
    }
    auto& level = it->second;
    level.remove(&record);
    if (level.empty()) {
      shard.asks.erase(it);
    }
  }
}

}  // namespace matcher
}  // namespace tradecore
