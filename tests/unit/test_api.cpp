#include "test_api.hpp"

#include <cassert>
#include "tradecore/api/api_router.hpp"

namespace tradecore::tests {

void test_api_router() {
  api::ApiRouter router;
  router.register_endpoint("/orders");
  assert(router.has_endpoint("/orders"));
}

}  // namespace tradecore::tests
