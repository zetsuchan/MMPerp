#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace api {

struct ExpressFeedFrame {
  std::uint64_t wal_offset{0};
  std::vector<std::byte> payload{};
};

struct TradeMetadata {
  std::uint64_t wal_offset{0};
  common::OrderId order_id{};
  common::AccountId account{0};
  common::MarketId market{0};
  std::int64_t price{0};
  std::int64_t quantity{0};
  common::TimestampNs timestamp_ns{0};
};

struct NodeStatus {
  std::uint64_t chain_id{1};
  std::uint64_t block_number{0};
  std::uint64_t peer_connections{0};
  bool healthy{true};
};

struct NodeStateProvider {
  std::function<std::uint64_t()> chain_id{};
  std::function<std::uint64_t()> block_number{};
  std::function<std::uint64_t()> peer_connections{};
  std::function<bool()> healthy{};
};

class ApiRouter {
 public:
  static constexpr std::size_t kDefaultBufferCapacity = 4096;

  explicit ApiRouter(
      std::size_t express_feed_capacity = kDefaultBufferCapacity,
      std::size_t trade_metadata_capacity = kDefaultBufferCapacity);

  void register_endpoint(std::string name);
  [[nodiscard]] bool has_endpoint(const std::string& name) const;
  [[nodiscard]] std::size_t endpoint_count() const;

  void set_node_state_provider(NodeStateProvider provider);
  [[nodiscard]] NodeStatus node_status() const;

  [[nodiscard]] std::string eth_chain_id() const;
  [[nodiscard]] std::string eth_block_number() const;
  [[nodiscard]] std::string monmouth_node_status() const;
  [[nodiscard]] std::string rpc_result(std::string_view method) const;

  void push_express_feed_frame(ExpressFeedFrame frame);
  [[nodiscard]] std::vector<ExpressFeedFrame> get_express_feed_frames(std::uint64_t min_wal_offset) const;
  [[nodiscard]] std::size_t express_feed_frame_count() const;

  void push_trade_metadata(TradeMetadata metadata);
  [[nodiscard]] std::vector<TradeMetadata> get_trade_metadata(std::uint64_t min_wal_offset) const;
  [[nodiscard]] std::size_t trade_metadata_count() const;

 private:
  mutable std::shared_mutex mutex_{};

  std::unordered_set<std::string> endpoints_{};
  std::deque<ExpressFeedFrame> express_feed_frames_{};
  std::deque<TradeMetadata> trade_metadata_{};

  std::size_t express_feed_capacity_{kDefaultBufferCapacity};
  std::size_t trade_metadata_capacity_{kDefaultBufferCapacity};

  NodeStateProvider node_state_provider_{};
};

}  // namespace api
}  // namespace tradecore
