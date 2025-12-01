#include "test_matcher.hpp"

#include <cassert>
#include "tradecore/matcher/matching_engine.hpp"

namespace tradecore::tests {

void test_matching_engine() {
  matcher::MatchingEngine matcher;
  matcher.add_market(1);

  common::OrderId maker_id{.market = 1, .session = 1, .local = 1};
  common::OrderId taker_id{.market = 1, .session = 1, .local = 2};

  matcher::OrderRequest maker{
      .id = maker_id,
      .account = 1001,
      .side = common::Side::kSell,
      .quantity = 5,
      .price = 1000,
      .tif = common::TimeInForce::kGtc,
      .flags = common::kFlagsNone,
  };
  auto maker_res = matcher.submit(maker);
  assert(maker_res.accepted);
  assert(maker_res.resting);

  matcher::OrderRequest taker{
      .id = taker_id,
      .account = 1002,
      .side = common::Side::kBuy,
      .quantity = 3,
      .price = 1100,
      .tif = common::TimeInForce::kIoc,
      .flags = common::kFlagsNone,
  };
  auto taker_res = matcher.submit(taker);
  assert(taker_res.accepted);
  assert(!taker_res.resting);
  assert(!taker_res.fills.empty());
  assert(taker_res.fills.front().quantity == 3);

  matcher::CancelRequest cancel_req{.id = maker_id};
  auto cancel_res = matcher.cancel(cancel_req);
  assert(cancel_res.cancelled);
}

void test_hidden_orders() {
  matcher::MatchingEngine matcher;
  matcher.add_market(1);

  // Place a hidden sell order - not visible on book but still matches
  common::OrderId hidden_id{.market = 1, .session = 1, .local = 10};
  matcher::OrderRequest hidden_order{
      .id = hidden_id,
      .account = 2001,
      .side = common::Side::kSell,
      .quantity = 100,
      .price = 1000,
      .tif = common::TimeInForce::kGtc,
      .flags = common::kHidden,
  };
  auto hidden_res = matcher.submit(hidden_order);
  assert(hidden_res.accepted);
  assert(hidden_res.resting);

  // Place a regular visible order at same price
  common::OrderId visible_id{.market = 1, .session = 1, .local = 11};
  matcher::OrderRequest visible_order{
      .id = visible_id,
      .account = 2002,
      .side = common::Side::kSell,
      .quantity = 50,
      .price = 1000,
      .tif = common::TimeInForce::kGtc,
      .flags = common::kFlagsNone,
  };
  auto visible_res = matcher.submit(visible_order);
  assert(visible_res.accepted);
  assert(visible_res.resting);

  // Taker buy should match against hidden order first (FIFO)
  common::OrderId taker_id{.market = 1, .session = 1, .local = 12};
  matcher::OrderRequest taker{
      .id = taker_id,
      .account = 2003,
      .side = common::Side::kBuy,
      .quantity = 120,
      .price = 1000,
      .tif = common::TimeInForce::kIoc,
      .flags = common::kFlagsNone,
  };
  auto taker_res = matcher.submit(taker);
  assert(taker_res.accepted);
  assert(taker_res.fills.size() == 2);
  // First fill against hidden order (100 qty)
  assert(taker_res.fills[0].maker_order.local == 10);
  assert(taker_res.fills[0].quantity == 100);
  // Second fill against visible order (20 qty)
  assert(taker_res.fills[1].maker_order.local == 11);
  assert(taker_res.fills[1].quantity == 20);
}

void test_iceberg_orders() {
  matcher::MatchingEngine matcher;
  matcher.add_market(1);

  // Place iceberg order: total 100, display 25
  common::OrderId iceberg_id{.market = 1, .session = 1, .local = 20};
  matcher::OrderRequest iceberg{
      .id = iceberg_id,
      .account = 3001,
      .side = common::Side::kSell,
      .quantity = 100,
      .price = 1000,
      .display_quantity = 25,
      .tif = common::TimeInForce::kGtc,
      .flags = common::kIceberg,
  };
  auto iceberg_res = matcher.submit(iceberg);
  assert(iceberg_res.accepted);
  assert(iceberg_res.resting);

  // First taker takes 30 - should fill 30 from iceberg, leaving 70 remaining
  common::OrderId taker1_id{.market = 1, .session = 1, .local = 21};
  matcher::OrderRequest taker1{
      .id = taker1_id,
      .account = 3002,
      .side = common::Side::kBuy,
      .quantity = 30,
      .price = 1000,
      .tif = common::TimeInForce::kIoc,
      .flags = common::kFlagsNone,
  };
  auto taker1_res = matcher.submit(taker1);
  assert(taker1_res.accepted);
  assert(taker1_res.fills.size() == 1);
  assert(taker1_res.fills[0].quantity == 30);

  // Second taker takes 50 - should fill from remaining iceberg (70)
  common::OrderId taker2_id{.market = 1, .session = 1, .local = 22};
  matcher::OrderRequest taker2{
      .id = taker2_id,
      .account = 3003,
      .side = common::Side::kBuy,
      .quantity = 50,
      .price = 1000,
      .tif = common::TimeInForce::kIoc,
      .flags = common::kFlagsNone,
  };
  auto taker2_res = matcher.submit(taker2);
  assert(taker2_res.accepted);
  assert(taker2_res.fills.size() == 1);
  assert(taker2_res.fills[0].quantity == 50);

  // Third taker tries to take 30 - should only fill 20 (remaining from iceberg)
  common::OrderId taker3_id{.market = 1, .session = 1, .local = 23};
  matcher::OrderRequest taker3{
      .id = taker3_id,
      .account = 3004,
      .side = common::Side::kBuy,
      .quantity = 30,
      .price = 1000,
      .tif = common::TimeInForce::kIoc,
      .flags = common::kFlagsNone,
  };
  auto taker3_res = matcher.submit(taker3);
  assert(taker3_res.accepted);
  assert(taker3_res.fills.size() == 1);
  assert(taker3_res.fills[0].quantity == 20);  // Only 20 remaining from iceberg
}

void test_iceberg_validation() {
  matcher::MatchingEngine matcher;
  matcher.add_market(1);

  // Iceberg with display_quantity = 0 should be rejected
  common::OrderId invalid1_id{.market = 1, .session = 1, .local = 30};
  matcher::OrderRequest invalid1{
      .id = invalid1_id,
      .account = 4001,
      .side = common::Side::kSell,
      .quantity = 100,
      .price = 1000,
      .display_quantity = 0,  // Invalid: must be > 0
      .tif = common::TimeInForce::kGtc,
      .flags = common::kIceberg,
  };
  auto invalid1_res = matcher.submit(invalid1);
  assert(!invalid1_res.accepted);
  assert(invalid1_res.reject_code != 0);

  // Iceberg with display_quantity > quantity should be rejected
  common::OrderId invalid2_id{.market = 1, .session = 1, .local = 31};
  matcher::OrderRequest invalid2{
      .id = invalid2_id,
      .account = 4002,
      .side = common::Side::kSell,
      .quantity = 100,
      .price = 1000,
      .display_quantity = 150,  // Invalid: must be <= quantity
      .tif = common::TimeInForce::kGtc,
      .flags = common::kIceberg,
  };
  auto invalid2_res = matcher.submit(invalid2);
  assert(!invalid2_res.accepted);
  assert(invalid2_res.reject_code != 0);
}

}  // namespace tradecore::tests
