#include "tradecore/api/api_router.hpp"

#include <algorithm>
#include <utility>

namespace tradecore {
namespace api {

void ApiRouter::register_endpoint(std::string name) {
  endpoints_.push_back(std::move(name));
}

void ApiRouter::register_endpoint(std::string name, EndpointHandler handler) {
  endpoints_.push_back(name);
  handlers_[std::move(name)] = std::move(handler);
}

bool ApiRouter::has_endpoint(const std::string& name) const {
  for (const auto& endpoint : endpoints_) {
    if (endpoint == name) {
      return true;
    }
  }
  return false;
}

std::optional<std::vector<std::byte>> ApiRouter::handle_request(const std::string& endpoint, std::span<const std::byte> request) {
  auto it = handlers_.find(endpoint);
  if (it == handlers_.end()) {
    return std::nullopt;
  }
  return it->second(request);
}

void ApiRouter::push_express_feed_frame(ExpressFeedFrame frame) {
  express_feed_frames_.push_back(std::move(frame));
}

std::vector<ExpressFeedFrame> ApiRouter::get_express_feed_frames(std::uint64_t since_sequence) const {
  std::vector<ExpressFeedFrame> result;
  for (const auto& frame : express_feed_frames_) {
    if (frame.sequence >= since_sequence) {
      result.push_back(frame);
    }
  }
  return result;
}

void ApiRouter::push_trade_metadata(TradeMetadata metadata) {
  trade_metadata_.push_back(std::move(metadata));
}

std::vector<TradeMetadata> ApiRouter::get_trade_metadata(std::uint64_t since_wal_offset, std::size_t limit) const {
  std::vector<TradeMetadata> result;
  for (const auto& metadata : trade_metadata_) {
    if (metadata.wal_offset >= since_wal_offset) {
      result.push_back(metadata);
      if (result.size() >= limit) {
        break;
      }
    }
  }
  return result;
}

void ApiRouter::set_state_root(StateRoot root) {
  state_root_ = std::move(root);
}

std::optional<StateRoot> ApiRouter::get_state_root() const {
  return state_root_;
}

}  // namespace api
}  // namespace tradecore
