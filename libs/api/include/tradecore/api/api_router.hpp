#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace api {

struct ExpressFeedFrame {
  std::uint64_t sequence{0};
  std::uint64_t wal_offset{0};
  std::vector<std::byte> payload{};
};

struct TradeMetadata {
  common::OrderId order_id{};
  std::uint64_t timestamp_ns{0};
  std::uint64_t wal_offset{0};
  std::int64_t quantity{0};
  std::int64_t price{0};
  common::Side side{common::Side::kBuy};
};

struct StateRoot {
  std::uint64_t sequence{0};
  std::array<std::byte, 32> merkle_root{};
  std::uint64_t timestamp_ns{0};
};

using EndpointHandler = std::function<std::vector<std::byte>(std::span<const std::byte>)>;

class ApiRouter {
 public:
  void register_endpoint(std::string name);
  void register_endpoint(std::string name, EndpointHandler handler);
  [[nodiscard]] bool has_endpoint(const std::string& name) const;
  
  std::optional<std::vector<std::byte>> handle_request(const std::string& endpoint, std::span<const std::byte> request);

  void push_express_feed_frame(ExpressFeedFrame frame);
  [[nodiscard]] std::vector<ExpressFeedFrame> get_express_feed_frames(std::uint64_t since_sequence = 0) const;

  void push_trade_metadata(TradeMetadata metadata);
  [[nodiscard]] std::vector<TradeMetadata> get_trade_metadata(std::uint64_t since_wal_offset = 0, std::size_t limit = 100) const;

  void set_state_root(StateRoot root);
  [[nodiscard]] std::optional<StateRoot> get_state_root() const;

 private:
  std::vector<std::string> endpoints_{};
  std::unordered_map<std::string, EndpointHandler> handlers_{};
  
  std::vector<ExpressFeedFrame> express_feed_frames_{};
  std::vector<TradeMetadata> trade_metadata_{};
  std::optional<StateRoot> state_root_{};
};

}  // namespace api
}  // namespace tradecore
