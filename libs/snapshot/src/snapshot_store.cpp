#include "tradecore/snapshot/snapshot_store.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tradecore {
namespace snapshot {

namespace {
constexpr std::uint32_t kMagic = 0x5443534e;  // 'TCSN'
constexpr std::uint16_t kVersionLegacy = 1;
constexpr std::uint16_t kVersionChecksummed = 2;
constexpr std::uint32_t kFnvPrime = 16777619u;
constexpr std::uint32_t kFnvOffsetBasis = 2166136261u;

struct SnapshotHeader {
  std::uint32_t magic{kMagic};
  std::uint16_t version{kVersionChecksummed};
  std::uint16_t reserved{0};
  common::SequenceId sequence{0};
  std::uint32_t payload_size{0};
};

struct EncodedRecord {
  SnapshotHeader header{};
  std::vector<std::byte> payload{};
};

std::uint32_t checksum32(std::span<const std::byte> payload) noexcept {
  std::uint32_t hash = kFnvOffsetBasis;
  for (const auto& b : payload) {
    hash ^= static_cast<std::uint8_t>(b);
    hash *= kFnvPrime;
  }
  return hash;
}

std::size_t encoded_record_size(const EncodedRecord& record) {
  return sizeof(SnapshotHeader) + record.payload.size() + sizeof(std::uint32_t);
}

void write_record(std::ofstream& out, common::SequenceId sequence, std::span<const std::byte> payload) {
  SnapshotHeader header;
  header.sequence = sequence;
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

  const auto checksum = checksum32(payload);
  out.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
  if (!out) {
    throw std::runtime_error("failed to write snapshot checksum");
  }
}

bool read_next_record(std::ifstream& in, EncodedRecord& out_record) {
  SnapshotHeader header;
  in.read(reinterpret_cast<char*>(&header), sizeof(header));
  if (!in) {
    if (in.eof()) {
      return false;
    }
    throw std::runtime_error("truncated snapshot header");
  }

  if (header.magic != kMagic) {
    throw std::runtime_error("invalid snapshot magic");
  }
  if (header.version != kVersionLegacy && header.version != kVersionChecksummed) {
    throw std::runtime_error("unsupported snapshot version");
  }

  out_record.header = header;
  out_record.payload.resize(header.payload_size);
  if (header.payload_size > 0) {
    in.read(reinterpret_cast<char*>(out_record.payload.data()), static_cast<std::streamsize>(header.payload_size));
    if (!in) {
      throw std::runtime_error("truncated snapshot payload");
    }
  }

  if (header.version >= kVersionChecksummed) {
    std::uint32_t expected_checksum = 0;
    in.read(reinterpret_cast<char*>(&expected_checksum), sizeof(expected_checksum));
    if (!in) {
      throw std::runtime_error("truncated snapshot checksum");
    }
    const auto actual_checksum = checksum32(out_record.payload);
    if (expected_checksum != actual_checksum) {
      throw std::runtime_error("snapshot checksum mismatch");
    }
  }

  return true;
}

std::vector<EncodedRecord> load_records(const std::filesystem::path& file_path) {
  std::vector<EncodedRecord> records;
  if (!std::filesystem::exists(file_path)) {
    return records;
  }

  std::ifstream in(file_path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to open snapshot file for read: " + file_path.string());
  }

  EncodedRecord record;
  while (read_next_record(in, record)) {
    records.push_back(record);
  }
  return records;
}

}  // namespace

Store::Store() = default;

Store::Store(std::filesystem::path directory, StoreOptions options)
    : options_(options) {
  prepare(std::move(directory));
}

void Store::prepare(const std::filesystem::path& directory) {
  if (!std::filesystem::exists(directory)) {
    std::filesystem::create_directories(directory);
  }
  directory_ = directory;
  file_path_ = directory_ / "snapshot.tc";
}

void Store::set_options(StoreOptions options) {
  options_ = options;
}

void Store::persist(common::SequenceId sequence_id, std::span<const std::byte> payload) {
  if (directory_.empty()) {
    throw std::runtime_error("snapshot store directory not set");
  }

  std::ofstream out(file_path_, std::ios::binary | std::ios::app);
  if (!out) {
    throw std::runtime_error("failed to open snapshot file for write: " + file_path_.string());
  }

  write_record(out, sequence_id, payload);
  out.flush();
  if (!out) {
    throw std::runtime_error("failed to flush snapshot file");
  }

  compact_if_needed();
}

std::optional<SnapshotRecord> Store::latest() const {
  if (file_path_.empty() || !std::filesystem::exists(file_path_)) {
    return std::nullopt;
  }

  const auto records = load_records(file_path_);
  if (records.empty()) {
    return std::nullopt;
  }

  const auto& last = records.back();
  return SnapshotRecord{
      .sequence = last.header.sequence,
      .payload = last.payload,
  };
}

std::size_t Store::record_count() const {
  if (file_path_.empty() || !std::filesystem::exists(file_path_)) {
    return 0;
  }
  return load_records(file_path_).size();
}

void Store::compact_if_needed() {
  if (file_path_.empty() || !std::filesystem::exists(file_path_)) {
    return;
  }

  const bool file_limit_enabled = options_.max_file_bytes > 0;
  const bool record_limit_enabled = options_.max_records > 0;
  if (!file_limit_enabled && !record_limit_enabled) {
    return;
  }

  const auto file_size = static_cast<std::size_t>(std::filesystem::file_size(file_path_));
  bool needs_compaction = file_limit_enabled && file_size > options_.max_file_bytes;

  auto records = load_records(file_path_);
  if (record_limit_enabled && records.size() > options_.max_records) {
    needs_compaction = true;
  }

  if (!needs_compaction || records.empty()) {
    return;
  }

  std::size_t start = 0;
  if (record_limit_enabled && records.size() > options_.max_records) {
    start = records.size() - options_.max_records;
  }

  if (file_limit_enabled) {
    auto kept_size = [&](std::size_t start_index) {
      std::size_t total = 0;
      for (std::size_t i = start_index; i < records.size(); ++i) {
        total += encoded_record_size(records[i]);
      }
      return total;
    };

    while (start + 1 < records.size() && kept_size(start) > options_.max_file_bytes) {
      ++start;
    }
  }

  const auto tmp_path = file_path_.string() + ".tmp";
  std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("failed to open temporary snapshot file for compaction: " + tmp_path);
  }

  for (std::size_t i = start; i < records.size(); ++i) {
    write_record(out, records[i].header.sequence, records[i].payload);
  }

  out.flush();
  out.close();

  std::filesystem::remove(file_path_);
  std::filesystem::rename(tmp_path, file_path_);
}

}  // namespace snapshot
}  // namespace tradecore
