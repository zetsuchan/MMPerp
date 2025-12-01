#include "test_persistence.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <span>

#include "tradecore/replay/replay_driver.hpp"
#include "tradecore/snapshot/snapshot_store.hpp"
#include "tradecore/wal/wal_writer.hpp"

namespace tradecore::tests {

void test_persistence_replay() {
  namespace fs = std::filesystem;
  const auto tmp_root = fs::temp_directory_path() / "tradecore_tests";
  fs::remove_all(tmp_root);

  // Snapshot test
  snapshot::Store snapshot_store(tmp_root);
  struct SnapshotState {
    std::int64_t balance;
  } snapshot_state{.balance = 42};
  snapshot_store.persist(0, std::as_bytes(std::span(&snapshot_state, 1)));
  auto snap = snapshot_store.latest();
  assert(snap.has_value());
  assert(snap->sequence == 0);

  // WAL test
  const auto wal_path = tmp_root / "events.wal";
  wal::Writer writer(wal_path, 128);

  auto append_int = [&](std::int32_t value) {
    std::array<std::byte, sizeof(value)> payload{};
    std::memcpy(payload.data(), &value, sizeof(value));
    wal::RecordView rec{.header = {},
                        .payload = std::span<const std::byte>(payload.data(), payload.size())};
    writer.append(rec);
  };

  append_int(10);
  append_int(-5);
  writer.sync();

  // Replay test
  replay::Driver driver;
  driver.configure(tmp_root, wal_path);

  std::int64_t replay_balance = 0;
  driver.set_snapshot_handler(
      [&](common::SequenceId seq, std::span<const std::byte> payload) {
        assert(seq == 0);
        assert(payload.size() == sizeof(SnapshotState));
        std::memcpy(&replay_balance, payload.data(), payload.size());
      });

  driver.set_event_handler([&](const wal::Record& record) {
    std::int32_t delta = 0;
    if (!record.payload.empty()) {
      std::memcpy(&delta, record.payload.data(), sizeof(delta));
    }
    replay_balance += delta;
  });

  driver.execute();
  assert(replay_balance == 47);

  fs::remove_all(tmp_root);
}

}  // namespace tradecore::tests
