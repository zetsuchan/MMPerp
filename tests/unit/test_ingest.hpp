#pragma once

namespace tradecore::tests {
void test_ingress_pipeline();
void test_cancel_message();
void test_heartbeat_dropped();
void test_rate_limiting();
void test_sbe_decode_bounds();
}  // namespace tradecore::tests
