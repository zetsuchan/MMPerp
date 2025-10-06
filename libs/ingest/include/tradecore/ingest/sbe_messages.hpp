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

}  // namespace sbe
}  // namespace ingest
}  // namespace tradecore
