#pragma once

#include <filesystem>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace snapshot {

class SnapshotStore {
 public:
  void prepare(const std::filesystem::path& directory);
  void persist(common::SequenceId sequence_id);

 private:
  std::filesystem::path directory_{};
};

}  // namespace snapshot
}  // namespace tradecore
