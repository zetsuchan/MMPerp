#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <span>
#include <vector>

namespace tradecore {
namespace wal {

struct RecordHeader {
  std::uint32_t magic{0x5443574c};      // 'TCWL'
  std::uint16_t version{1};
  std::uint16_t reserved{0};
  std::uint64_t sequence{0};
  std::uint32_t payload_size{0};
  std::uint32_t checksum{0};
};

struct RecordView {
  RecordHeader header{};
  std::span<const std::byte> payload{};
};

struct Record {
  RecordHeader header{};
  std::vector<std::byte> payload{};
};

class Writer {
 public:
  explicit Writer(const std::filesystem::path& path,
                  std::size_t flush_threshold_bytes = 1 << 16);
  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;
  Writer(Writer&&) = delete;
  Writer& operator=(Writer&&) = delete;
  ~Writer();

  void append(const RecordView& record);
  void flush();
  void sync();
  [[nodiscard]] std::uint64_t next_sequence() const noexcept { return next_sequence_; }

 private:
  std::FILE* file_{nullptr};
  std::vector<std::byte> buffer_{};
  std::size_t flush_threshold_;
  std::uint64_t next_sequence_{1};

  void ensure_open(const std::filesystem::path& path);
};

class Reader {
 public:
  explicit Reader(const std::filesystem::path& path);
  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;
  Reader(Reader&&) = delete;
  Reader& operator=(Reader&&) = delete;
  ~Reader();

  bool next(Record& out_record);
  void seek_sequence(std::uint64_t sequence);

 private:
  std::FILE* file_{nullptr};
  std::filesystem::path path_{};
};

}  // namespace wal
}  // namespace tradecore
