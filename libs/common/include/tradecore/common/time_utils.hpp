#pragma once

#include <chrono>

namespace tradecore {
namespace common {

inline std::chrono::nanoseconds now_steady() noexcept {
  return std::chrono::steady_clock::now().time_since_epoch();
}

}  // namespace common
}  // namespace tradecore
