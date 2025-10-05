#include "tradecore/replay/replay_driver.hpp"

#include <filesystem>
#include <stdexcept>

namespace tradecore {
namespace replay {

Driver::Driver() = default;

void Driver::configure(std::filesystem::path snapshot_directory, std::filesystem::path wal_path) {
  snapshot_store_.prepare(snapshot_directory);
  wal_path_ = std::move(wal_path);
}

void Driver::set_snapshot_handler(SnapshotHandler handler) {
  snapshot_handler_ = std::move(handler);
}

void Driver::set_event_handler(EventHandler handler) {
  event_handler_ = std::move(handler);
}

void Driver::execute() {
  if (!event_handler_) {
    throw std::runtime_error("event handler not set for replay");
  }

  common::SequenceId resume_from{1};

  if (auto snap = snapshot_store_.latest()) {
    resume_from = snap->sequence + 1;
    if (snapshot_handler_) {
      snapshot_handler_(snap->sequence, std::span<const std::byte>(snap->payload.data(), snap->payload.size()));
    }
  }

  if (!std::filesystem::exists(wal_path_)) {
    return;
  }

  wal::Reader reader(wal_path_);
  wal::Record record;
  while (reader.next(record)) {
    if (record.header.sequence < resume_from) {
      continue;
    }
    event_handler_(record);
  }
}

}  // namespace replay
}  // namespace tradecore
