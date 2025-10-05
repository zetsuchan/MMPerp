#include "tradecore/snapshot/snapshot_store.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace tradecore {
namespace snapshot {

namespace {
constexpr std::uint32_t kMagic = 0x5443534e;  // 'TCSN'

struct SnapshotHeader {
  std::uint32_t magic{kMagic};
  std::uint16_t version{1};
  std::uint16_t reserved{0};
  common::SequenceId sequence{0};
  std::uint32_t payload_size{0};
};

}  // namespace

Store::Store() = default;

Store::Store(std::filesystem::path directory) {
  prepare(std::move(directory));
}

void Store::prepare(const std::filesystem::path& directory) {
  if (!std::filesystem::exists(directory)) {
    std::filesystem::create_directories(directory);
  }
  directory_ = directory;
  file_path_ = directory_ / "snapshot.tc";
}

void Store::persist(common::SequenceId sequence_id, std::span<const std::byte> payload) {
  if (directory_.empty()) {
    throw std::runtime_error("snapshot store directory not set");
  }

  std::ofstream out(file_path_, std::ios::binary | std::ios::app);
  if (!out) {
    throw std::runtime_error("failed to open snapshot file for write: " + file_path_.string());
  }

  SnapshotHeader header;
  header.sequence = sequence_id;
  header.payload_size = static_cast<std::uint32_t>(payload.size());

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  if (!out) {
    throw std::runtime_error("failed to write snapshot header");
  }
  if (!payload.empty()) {
    out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    if (!out) {
      throw std::runtime_error("failed to write snapshot payload");
    }
  }
  out.flush();
}

std::optional<SnapshotRecord> Store::latest() const {
  if (file_path_.empty() || !std::filesystem::exists(file_path_)) {
    return std::nullopt;
  }

  std::ifstream in(file_path_, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to open snapshot file for read: " + file_path_.string());
  }

  SnapshotHeader header;
  SnapshotRecord record;

  while (in.read(reinterpret_cast<char*>(&header), sizeof(header))) {
    if (header.magic != kMagic) {
      throw std::runtime_error("invalid snapshot magic");
    }
    record.sequence = header.sequence;
    record.payload.resize(header.payload_size);
    if (header.payload_size > 0) {
      in.read(reinterpret_cast<char*>(record.payload.data()), static_cast<std::streamsize>(header.payload_size));
      if (!in) {
        throw std::runtime_error("truncated snapshot record");
      }
    }
  }

  if (record.sequence == 0 && record.payload.empty()) {
    return std::nullopt;
  }
  return record;
}

}  // namespace snapshot
}  // namespace tradecore
