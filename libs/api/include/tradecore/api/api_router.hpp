#pragma once

#include <string>
#include <vector>

namespace tradecore {
namespace api {

class ApiRouter {
 public:
  void register_endpoint(std::string name);
  [[nodiscard]] bool has_endpoint(const std::string& name) const;

 private:
  std::vector<std::string> endpoints_{};
};

}  // namespace api
}  // namespace tradecore
