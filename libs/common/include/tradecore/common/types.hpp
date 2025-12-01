#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace tradecore {
namespace common {

using MarketId = std::uint16_t;
using SessionId = std::uint16_t;
using SequenceId = std::uint32_t;
using TimestampNs = std::int64_t;
using AccountId = std::uint64_t;

enum class Side : std::uint8_t {
  kBuy,
  kSell,
};

enum class TimeInForce : std::uint8_t {
  kGtc,
  kIoc,
  kFok,
  kGoodTilBlock,
  kGoodTilTime,
};

enum OrderFlags : std::uint16_t {
  kFlagsNone = 0,
  kPostOnly = 1 << 0,
  kReduceOnly = 1 << 1,
  kHidden = 1 << 2,    // Fully hidden order - not visible on book, still matches
  kIceberg = 1 << 3,   // Iceberg order - shows display_quantity, hides rest
};

inline constexpr bool HasFlag(std::uint16_t flags, OrderFlags flag) noexcept {
  return (flags & static_cast<std::uint16_t>(flag)) != 0;
}

struct OrderId {
  MarketId market{};
  SessionId session{};
  SequenceId local{};

  [[nodiscard]] std::uint64_t value() const noexcept {
    return (static_cast<std::uint64_t>(market) << 48) |
           (static_cast<std::uint64_t>(session) << 32) |
           static_cast<std::uint64_t>(local);
  }
};

struct EngineId {
  std::string name;
  std::string_view version;
};

}  // namespace common
}  // namespace tradecore
