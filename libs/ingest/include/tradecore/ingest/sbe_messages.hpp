#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace ingest {
namespace sbe {

struct NewOrder {
  common::Side side{common::Side::kBuy};
  std::int64_t quantity{0};
  std::int64_t price{0};
  std::uint16_t flags{0};
};

struct Cancel {
  std::uint64_t order_id{0};
};

struct Replace {
  std::uint64_t order_id{0};
  std::int64_t new_quantity{0};
  std::int64_t new_price{0};
  std::uint16_t new_flags{0};
};

struct FillEvent {
  std::uint64_t maker_order_id{0};
  std::uint64_t taker_order_id{0};
  std::int64_t quantity{0};
  std::int64_t price{0};
  std::uint64_t timestamp_ns{0};
};

struct OrderAck {
  std::uint64_t order_id{0};
  std::uint8_t accepted{0};
  std::uint8_t resting{0};
  std::uint16_t reject_code{0};
};

struct OrderReject {
  std::uint64_t order_id{0};
  std::uint16_t reject_code{0};
};

struct CancelAck {
  std::uint64_t order_id{0};
  std::uint8_t cancelled{0};
  std::uint16_t reject_code{0};
};

struct Heartbeat {
  std::uint64_t timestamp_ns{0};
  std::uint64_t sequence{0};
};

namespace detail {

template <typename T>
inline void append_primitive(std::vector<std::byte>& buffer, T value) {
  auto raw = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
  buffer.insert(buffer.end(), raw.begin(), raw.end());
}

template <typename T>
inline T read_primitive(std::span<const std::byte> data, std::size_t& offset) {
  if (offset + sizeof(T) > data.size()) {
    throw std::runtime_error("sbe decode out of bounds");
  }
  std::array<std::byte, sizeof(T)> storage{};
  std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(offset), sizeof(T), storage.begin());
  offset += sizeof(T);
  return std::bit_cast<T>(storage);
}

}  // namespace detail

inline std::vector<std::byte> encode(const NewOrder& msg) {
  std::vector<std::byte> buffer;
  buffer.reserve(1 + sizeof(std::int64_t) * 2 + sizeof(std::uint16_t));
  detail::append_primitive<std::uint8_t>(buffer, static_cast<std::uint8_t>(msg.side));
  detail::append_primitive<std::int64_t>(buffer, msg.quantity);
  detail::append_primitive<std::int64_t>(buffer, msg.price);
  detail::append_primitive<std::uint16_t>(buffer, msg.flags);
  return buffer;
}

inline NewOrder decode_new_order(std::span<const std::byte> data) {
  std::size_t offset = 0;
  NewOrder msg;
  msg.side = static_cast<common::Side>(detail::read_primitive<std::uint8_t>(data, offset));
  msg.quantity = detail::read_primitive<std::int64_t>(data, offset);
  msg.price = detail::read_primitive<std::int64_t>(data, offset);
  msg.flags = detail::read_primitive<std::uint16_t>(data, offset);
  return msg;
}

inline std::vector<std::byte> encode(const Cancel& msg) {
  std::vector<std::byte> buffer;
  buffer.reserve(sizeof(std::uint64_t));
  detail::append_primitive<std::uint64_t>(buffer, msg.order_id);
  return buffer;
}

inline Cancel decode_cancel(std::span<const std::byte> data) {
  std::size_t offset = 0;
  Cancel msg;
  msg.order_id = detail::read_primitive<std::uint64_t>(data, offset);
  return msg;
}

inline std::vector<std::byte> encode(const Replace& msg) {
  std::vector<std::byte> buffer;
  buffer.reserve(sizeof(std::uint64_t) + sizeof(std::int64_t) * 2 + sizeof(std::uint16_t));
  detail::append_primitive<std::uint64_t>(buffer, msg.order_id);
  detail::append_primitive<std::int64_t>(buffer, msg.new_quantity);
  detail::append_primitive<std::int64_t>(buffer, msg.new_price);
  detail::append_primitive<std::uint16_t>(buffer, msg.new_flags);
  return buffer;
}

inline Replace decode_replace(std::span<const std::byte> data) {
  std::size_t offset = 0;
  Replace msg;
  msg.order_id = detail::read_primitive<std::uint64_t>(data, offset);
  msg.new_quantity = detail::read_primitive<std::int64_t>(data, offset);
  msg.new_price = detail::read_primitive<std::int64_t>(data, offset);
  msg.new_flags = detail::read_primitive<std::uint16_t>(data, offset);
  return msg;
}

inline std::vector<std::byte> encode(const FillEvent& msg) {
  std::vector<std::byte> buffer;
  buffer.reserve(sizeof(std::uint64_t) * 3 + sizeof(std::int64_t) * 2);
  detail::append_primitive<std::uint64_t>(buffer, msg.maker_order_id);
  detail::append_primitive<std::uint64_t>(buffer, msg.taker_order_id);
  detail::append_primitive<std::int64_t>(buffer, msg.quantity);
  detail::append_primitive<std::int64_t>(buffer, msg.price);
  detail::append_primitive<std::uint64_t>(buffer, msg.timestamp_ns);
  return buffer;
}

inline FillEvent decode_fill_event(std::span<const std::byte> data) {
  std::size_t offset = 0;
  FillEvent msg;
  msg.maker_order_id = detail::read_primitive<std::uint64_t>(data, offset);
  msg.taker_order_id = detail::read_primitive<std::uint64_t>(data, offset);
  msg.quantity = detail::read_primitive<std::int64_t>(data, offset);
  msg.price = detail::read_primitive<std::int64_t>(data, offset);
  msg.timestamp_ns = detail::read_primitive<std::uint64_t>(data, offset);
  return msg;
}

inline std::vector<std::byte> encode(const OrderAck& msg) {
  std::vector<std::byte> buffer;
  buffer.reserve(sizeof(std::uint64_t) + sizeof(std::uint8_t) * 2 + sizeof(std::uint16_t));
  detail::append_primitive<std::uint64_t>(buffer, msg.order_id);
  detail::append_primitive<std::uint8_t>(buffer, msg.accepted);
  detail::append_primitive<std::uint8_t>(buffer, msg.resting);
  detail::append_primitive<std::uint16_t>(buffer, msg.reject_code);
  return buffer;
}

inline OrderAck decode_order_ack(std::span<const std::byte> data) {
  std::size_t offset = 0;
  OrderAck msg;
  msg.order_id = detail::read_primitive<std::uint64_t>(data, offset);
  msg.accepted = detail::read_primitive<std::uint8_t>(data, offset);
  msg.resting = detail::read_primitive<std::uint8_t>(data, offset);
  msg.reject_code = detail::read_primitive<std::uint16_t>(data, offset);
  return msg;
}

inline std::vector<std::byte> encode(const OrderReject& msg) {
  std::vector<std::byte> buffer;
  buffer.reserve(sizeof(std::uint64_t) + sizeof(std::uint16_t));
  detail::append_primitive<std::uint64_t>(buffer, msg.order_id);
  detail::append_primitive<std::uint16_t>(buffer, msg.reject_code);
  return buffer;
}

inline OrderReject decode_order_reject(std::span<const std::byte> data) {
  std::size_t offset = 0;
  OrderReject msg;
  msg.order_id = detail::read_primitive<std::uint64_t>(data, offset);
  msg.reject_code = detail::read_primitive<std::uint16_t>(data, offset);
  return msg;
}

inline std::vector<std::byte> encode(const CancelAck& msg) {
  std::vector<std::byte> buffer;
  buffer.reserve(sizeof(std::uint64_t) + sizeof(std::uint8_t) + sizeof(std::uint16_t));
  detail::append_primitive<std::uint64_t>(buffer, msg.order_id);
  detail::append_primitive<std::uint8_t>(buffer, msg.cancelled);
  detail::append_primitive<std::uint16_t>(buffer, msg.reject_code);
  return buffer;
}

inline CancelAck decode_cancel_ack(std::span<const std::byte> data) {
  std::size_t offset = 0;
  CancelAck msg;
  msg.order_id = detail::read_primitive<std::uint64_t>(data, offset);
  msg.cancelled = detail::read_primitive<std::uint8_t>(data, offset);
  msg.reject_code = detail::read_primitive<std::uint16_t>(data, offset);
  return msg;
}

inline std::vector<std::byte> encode(const Heartbeat& msg) {
  std::vector<std::byte> buffer;
  buffer.reserve(sizeof(std::uint64_t) * 2);
  detail::append_primitive<std::uint64_t>(buffer, msg.timestamp_ns);
  detail::append_primitive<std::uint64_t>(buffer, msg.sequence);
  return buffer;
}

inline Heartbeat decode_heartbeat(std::span<const std::byte> data) {
  std::size_t offset = 0;
  Heartbeat msg;
  msg.timestamp_ns = detail::read_primitive<std::uint64_t>(data, offset);
  msg.sequence = detail::read_primitive<std::uint64_t>(data, offset);
  return msg;
}

}  // namespace sbe
}  // namespace ingest
}  // namespace tradecore
