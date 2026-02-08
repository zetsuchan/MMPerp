#include "tradecore/api/api_router.hpp"

#include <algorithm>
#include <iomanip>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <utility>

namespace tradecore {
namespace api {

namespace {

std::string to_hex(std::uint64_t value) {
  std::ostringstream out;
  out << "0x" << std::hex << std::nouppercase << value;
  return out.str();
}

template <typename T>
void push_with_fifo_eviction(std::deque<T>& buffer, std::size_t max_size, T value) {
  if (max_size == 0) {
    return;
  }
  while (buffer.size() >= max_size) {
    buffer.pop_front();
  }
  buffer.push_back(std::move(value));
}

}  // namespace

ApiRouter::ApiRouter(
    std::size_t express_feed_capacity,
    std::size_t trade_metadata_capacity)
    : express_feed_capacity_(std::max<std::size_t>(1, express_feed_capacity)),
      trade_metadata_capacity_(std::max<std::size_t>(1, trade_metadata_capacity)) {}

void ApiRouter::register_endpoint(std::string name) {
  if (name.empty()) {
    return;
  }
  std::unique_lock lock(mutex_);
  endpoints_.insert(std::move(name));
}

bool ApiRouter::has_endpoint(const std::string& name) const {
  std::shared_lock lock(mutex_);
  return endpoints_.contains(name);
}

std::size_t ApiRouter::endpoint_count() const {
  std::shared_lock lock(mutex_);
  return endpoints_.size();
}

void ApiRouter::set_node_state_provider(NodeStateProvider provider) {
  std::unique_lock lock(mutex_);
  node_state_provider_ = std::move(provider);
}

NodeStatus ApiRouter::node_status() const {
  NodeStateProvider provider;
  {
    std::shared_lock lock(mutex_);
    provider = node_state_provider_;
  }

  NodeStatus status;
  if (provider.chain_id) {
    status.chain_id = provider.chain_id();
  }
  if (provider.block_number) {
    status.block_number = provider.block_number();
  }
  if (provider.peer_connections) {
    status.peer_connections = provider.peer_connections();
  }
  if (provider.healthy) {
    status.healthy = provider.healthy();
  }
  return status;
}

std::string ApiRouter::eth_chain_id() const {
  return to_hex(node_status().chain_id);
}

std::string ApiRouter::eth_block_number() const {
  return to_hex(node_status().block_number);
}

std::string ApiRouter::monmouth_node_status() const {
  const auto status = node_status();
  std::ostringstream out;
  out << "{\"healthy\":" << (status.healthy ? "true" : "false")
      << ",\"chainId\":\"" << to_hex(status.chain_id) << "\""
      << ",\"blockNumber\":\"" << to_hex(status.block_number) << "\""
      << ",\"peerConnections\":" << status.peer_connections << "}";
  return out.str();
}

std::string ApiRouter::rpc_result(std::string_view method) const {
  if (method == "eth_chainId") {
    return eth_chain_id();
  }
  if (method == "eth_blockNumber") {
    return eth_block_number();
  }
  if (method == "monmouth_nodeStatus") {
    return monmouth_node_status();
  }
  return "{\"error\":\"method not found\"}";
}

void ApiRouter::push_express_feed_frame(ExpressFeedFrame frame) {
  std::unique_lock lock(mutex_);
  push_with_fifo_eviction(express_feed_frames_, express_feed_capacity_, std::move(frame));
}

std::vector<ExpressFeedFrame> ApiRouter::get_express_feed_frames(std::uint64_t min_wal_offset) const {
  std::shared_lock lock(mutex_);
  std::vector<ExpressFeedFrame> output;
  output.reserve(express_feed_frames_.size());
  for (const auto& frame : express_feed_frames_) {
    if (frame.wal_offset >= min_wal_offset) {
      output.push_back(frame);
    }
  }
  return output;
}

std::size_t ApiRouter::express_feed_frame_count() const {
  std::shared_lock lock(mutex_);
  return express_feed_frames_.size();
}

void ApiRouter::push_trade_metadata(TradeMetadata metadata) {
  std::unique_lock lock(mutex_);
  push_with_fifo_eviction(trade_metadata_, trade_metadata_capacity_, std::move(metadata));
}

std::vector<TradeMetadata> ApiRouter::get_trade_metadata(std::uint64_t min_wal_offset) const {
  std::shared_lock lock(mutex_);
  std::vector<TradeMetadata> output;
  output.reserve(trade_metadata_.size());
  for (const auto& metadata : trade_metadata_) {
    if (metadata.wal_offset >= min_wal_offset) {
      output.push_back(metadata);
    }
  }
  return output;
}

std::size_t ApiRouter::trade_metadata_count() const {
  std::shared_lock lock(mutex_);
  return trade_metadata_.size();
}

}  // namespace api
}  // namespace tradecore
