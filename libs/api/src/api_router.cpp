#include "tradecore/api/api_router.hpp"

#include <utility>

namespace tradecore {
namespace api {

void ApiRouter::register_endpoint(std::string name) {
  endpoints_.push_back(std::move(name));
}

bool ApiRouter::has_endpoint(const std::string& name) const {
  for (const auto& endpoint : endpoints_) {
    if (endpoint == name) {
      return true;
    }
  }
  return false;
}

}  // namespace api
}  // namespace tradecore
