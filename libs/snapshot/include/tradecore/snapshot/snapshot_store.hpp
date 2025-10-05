#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace snapshot {

struct SnapshotRecord {
  common::SequenceId sequence{0};
  std::vector<std::byte> payload{};
};

class Store {
 public:
  Store();
  explicit Store(std::filesystem::path directory);

  void prepare(const std::filesystem::path& directory);
  void persist(common::SequenceId sequence_id, std::span<const std::byte> payload);
  [[nodiscard]] std::optional<SnapshotRecord> latest() const;
  [[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }

 private:
  std::filesystem::path directory_{};
  std::filesystem::path file_path_{};
};

}  // namespace snapshot
}  // namespace tradecore
