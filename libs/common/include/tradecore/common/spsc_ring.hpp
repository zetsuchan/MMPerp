#pragma once

#include <atomic>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace tradecore {
namespace common {

template <typename T>
class SpscRing {
 public:
  explicit SpscRing(std::size_t capacity_power_of_two)
      : buffer_(capacity_power_of_two), mask_(capacity_power_of_two - 1) {
    if (capacity_power_of_two == 0 || (capacity_power_of_two & mask_) != 0) {
      throw std::invalid_argument("SpscRing capacity must be power of two");
    }
  }

  bool push(T value) {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t next_head = (head + 1) & mask_;
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    if (next_head == tail) {
      return false;  // full
    }
    buffer_[head] = std::move(value);
    head_.store(next_head, std::memory_order_release);
    return true;
  }

  bool pop(T& out) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    const std::size_t head = head_.load(std::memory_order_acquire);
    if (tail == head) {
      return false;  // empty
    }
    out = std::move(*buffer_[tail]);
    buffer_[tail].reset();
    tail_.store((tail + 1) & mask_, std::memory_order_release);
    return true;
  }

  bool empty() const {
    return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
  }

 private:
  std::vector<std::optional<T>> buffer_;
  const std::size_t mask_;
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};
};

}  // namespace common
}  // namespace tradecore
