#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace ingest {

enum class MessageKind : std::uint8_t {
  kNewOrder,
  kCancel,
  kReplace,
  kHeartbeat,
};

struct FrameHeader {
  common::AccountId account{0};
  std::uint64_t nonce{0};
  common::TimestampNs received_time_ns{0};
  std::uint8_t priority{0};
  MessageKind kind{MessageKind::kNewOrder};
};

struct Frame {
  FrameHeader header;
  std::span<const std::byte> payload;
};

struct OwnedFrame {
  FrameHeader header;
  std::vector<std::byte> payload;
};

}  // namespace ingest
}  // namespace tradecore
