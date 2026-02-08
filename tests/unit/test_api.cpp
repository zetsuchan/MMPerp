#include "test_api.hpp"

#include <atomic>
#include <cassert>
#include <string>

#include "tradecore/api/api_router.hpp"

namespace tradecore::tests {

void test_api_router() {
  api::ApiRouter router;
  router.register_endpoint("/orders");
  router.register_endpoint("/orders");
  assert(router.has_endpoint("/orders"));
  assert(router.endpoint_count() == 1);

  std::atomic<std::uint64_t> chain_id{8453};
  std::atomic<std::uint64_t> block_number{42};
  std::atomic<std::uint64_t> peers{3};
  std::atomic<bool> healthy{true};

  router.set_node_state_provider({
      .chain_id = [&chain_id]() { return chain_id.load(); },
      .block_number = [&block_number]() { return block_number.load(); },
      .peer_connections = [&peers]() { return peers.load(); },
      .healthy = [&healthy]() { return healthy.load(); },
  });

  assert(router.rpc_result("eth_chainId") == "0x2105");
  assert(router.rpc_result("eth_blockNumber") == "0x2a");
  const std::string node_status = router.rpc_result("monmouth_nodeStatus");
  assert(node_status.find("\"chainId\":\"0x2105\"") != std::string::npos);
  assert(node_status.find("\"blockNumber\":\"0x2a\"") != std::string::npos);
  assert(node_status.find("\"peerConnections\":3") != std::string::npos);

  block_number.store(255);
  assert(router.rpc_result("eth_blockNumber") == "0xff");
  assert(router.rpc_result("unknown_method").find("method not found") != std::string::npos);

  api::ApiRouter bounded_router(2, 2);
  bounded_router.push_express_feed_frame({.wal_offset = 1});
  bounded_router.push_express_feed_frame({.wal_offset = 2});
  bounded_router.push_express_feed_frame({.wal_offset = 3});

  const auto frames = bounded_router.get_express_feed_frames(0);
  assert(frames.size() == 2);
  assert(frames[0].wal_offset == 2);
  assert(frames[1].wal_offset == 3);

  bounded_router.push_trade_metadata({.wal_offset = 7});
  bounded_router.push_trade_metadata({.wal_offset = 8});
  bounded_router.push_trade_metadata({.wal_offset = 9});

  const auto metadata = bounded_router.get_trade_metadata(8);
  assert(bounded_router.trade_metadata_count() == 2);
  assert(metadata.size() == 2);
  assert(metadata[0].wal_offset == 8);
  assert(metadata[1].wal_offset == 9);
}

}  // namespace tradecore::tests
