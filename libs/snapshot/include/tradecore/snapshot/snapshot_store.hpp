#pragma once

#include <cstddef>
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

struct StoreOptions {
  std::size_t max_records{1024};
  std::size_t max_file_bytes{64u << 20};  // 64 MiB
};

class Store {
 public:
  Store();
  explicit Store(std::filesystem::path directory, StoreOptions options = {});

  void prepare(const std::filesystem::path& directory);
  void set_options(StoreOptions options);
  void persist(common::SequenceId sequence_id, std::span<const std::byte> payload);
  [[nodiscard]] std::optional<SnapshotRecord> latest() const;
  [[nodiscard]] std::size_t record_count() const;
  [[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }

 private:
  std::filesystem::path directory_{};
  std::filesystem::path file_path_{};
  StoreOptions options_{};

  void compact_if_needed();
};

}  // namespace snapshot
}  // namespace tradecore
