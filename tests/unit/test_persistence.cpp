#include "test_persistence.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>

#include "tradecore/replay/replay_driver.hpp"
#include "tradecore/snapshot/snapshot_store.hpp"
#include "tradecore/wal/wal_writer.hpp"

namespace tradecore::tests {

namespace {
namespace fs = std::filesystem;

struct SnapshotState {
  std::int64_t balance;
};

void write_fixture(const fs::path& tmp_root, const fs::path& wal_path) {
  snapshot::Store snapshot_store(tmp_root);
  SnapshotState snapshot_state{.balance = 42};
  snapshot_store.persist(0, std::as_bytes(std::span(&snapshot_state, 1)));

  wal::Writer writer(wal_path, 128);
  auto append_int = [&](std::int32_t value) {
    std::array<std::byte, sizeof(value)> payload{};
    std::memcpy(payload.data(), &value, sizeof(value));
    writer.append({
        .payload = std::span<const std::byte>(payload.data(), payload.size()),
    });
  };

  append_int(10);
  append_int(-5);
  writer.sync();
}

std::int64_t replay_balance_from_fixture(const fs::path& tmp_root, const fs::path& wal_path) {
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
  return replay_balance;
}
}  // namespace

void test_persistence_replay() {
  namespace fs = std::filesystem;
  const auto tmp_root = fs::temp_directory_path() / "tradecore_tests";
  fs::remove_all(tmp_root);
  const auto wal_path = tmp_root / "events.wal";
  write_fixture(tmp_root, wal_path);

  snapshot::Store snapshot_store(tmp_root);
  auto snap = snapshot_store.latest();
  assert(snap.has_value());
  assert(snap->sequence == 0);

  const auto replay_balance = replay_balance_from_fixture(tmp_root, wal_path);
  assert(replay_balance == 47);

  fs::remove_all(tmp_root);
}

void test_persistence_replay_determinism() {
  namespace fs = std::filesystem;
  const auto tmp_root = fs::temp_directory_path() / "tradecore_tests_determinism";
  fs::remove_all(tmp_root);
  const auto wal_path = tmp_root / "events.wal";
  write_fixture(tmp_root, wal_path);

  const auto first = replay_balance_from_fixture(tmp_root, wal_path);
  const auto second = replay_balance_from_fixture(tmp_root, wal_path);

  assert(first == second);
  assert(first == 47);
  fs::remove_all(tmp_root);
}

void test_snapshot_compaction_and_integrity() {
  namespace fs = std::filesystem;
  const auto tmp_root = fs::temp_directory_path() / "tradecore_tests_snapshot_compaction";
  fs::remove_all(tmp_root);

  snapshot::Store store(tmp_root, {
                                      .max_records = 3,
                                      .max_file_bytes = 0,
                                  });

  for (common::SequenceId seq = 1; seq <= 5; ++seq) {
    std::array<std::byte, sizeof(seq)> payload{};
    std::memcpy(payload.data(), &seq, sizeof(seq));
    store.persist(seq, std::span<const std::byte>(payload.data(), payload.size()));
  }

  assert(store.record_count() == 3);
  const auto latest = store.latest();
  assert(latest.has_value());
  assert(latest->sequence == 5);

  common::SequenceId decoded_latest = 0;
  std::memcpy(&decoded_latest, latest->payload.data(), sizeof(decoded_latest));
  assert(decoded_latest == 5);

  const auto file_path = tmp_root / "snapshot.tc";
  {
    std::fstream io(file_path, std::ios::binary | std::ios::in | std::ios::out);
    assert(io.good());
    io.seekg(-1, std::ios::end);
    char trailing = 0;
    io.read(&trailing, 1);
    trailing ^= static_cast<char>(0x01);
    io.seekp(-1, std::ios::end);
    io.write(&trailing, 1);
  }

  bool checksum_failed = false;
  try {
    (void)store.latest();
  } catch (const std::runtime_error&) {
    checksum_failed = true;
  }
  assert(checksum_failed);

  fs::remove_all(tmp_root);
}

}  // namespace tradecore::tests
