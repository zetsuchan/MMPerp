#pragma once

#include <filesystem>
#include <span>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace wal {

class WalWriter {
 public:
  void open(const std::filesystem::path& path);
  void append(std::span<const std::byte> payload);
  void flush();

 private:
  std::filesystem::path path_{};
};

}  // namespace wal
}  // namespace tradecore
