#pragma once

#include <filesystem>
#include <functional>
#include <span>

#include "tradecore/common/types.hpp"
#include "tradecore/snapshot/snapshot_store.hpp"
#include "tradecore/wal/wal_writer.hpp"

namespace tradecore {
namespace replay {

class Driver {
 public:
  using SnapshotHandler = std::function<void(common::SequenceId, std::span<const std::byte>)>;
  using EventHandler = std::function<void(const wal::Record&)>;

  Driver();

  void configure(std::filesystem::path snapshot_directory, std::filesystem::path wal_path);
  void set_snapshot_handler(SnapshotHandler handler);
  void set_event_handler(EventHandler handler);
  void execute();

 private:
  snapshot::Store snapshot_store_{};
  std::filesystem::path wal_path_{};
  SnapshotHandler snapshot_handler_{};
  EventHandler event_handler_{};
};

}  // namespace replay
}  // namespace tradecore
