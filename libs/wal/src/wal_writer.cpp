#include "tradecore/wal/wal_writer.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace tradecore {
namespace wal {

namespace {
constexpr std::uint32_t kMagic = 0x5443574c;  // 'TCWL'

std::uint32_t checksum32(std::span<const std::byte> data) noexcept {
  constexpr std::uint32_t kFnvPrime = 16777619u;
  std::uint32_t hash = 2166136261u;
  for (const auto& b : data) {
    hash ^= static_cast<std::uint8_t>(b);
    hash *= kFnvPrime;
  }
  return hash;
}

int get_fileno(std::FILE* file) {
#if defined(_WIN32)
  return _fileno(file);
#else
  return fileno(file);
#endif
}

void fsync_file(std::FILE* file) {
  const int fd = get_fileno(file);
#if defined(_WIN32)
  if (::FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(fd))) == 0) {
    throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "FlushFileBuffers failed");
  }
#else
  if (::fsync(fd) != 0) {
    throw std::system_error(errno, std::system_category(), "fsync failed");
  }
#endif
}

}  // namespace

Writer::Writer(const std::filesystem::path& path, std::size_t flush_threshold_bytes)
    : buffer_(), flush_threshold_(flush_threshold_bytes) {
  buffer_.reserve(flush_threshold_bytes);
  ensure_open(path);
}

Writer::~Writer() {
  try {
    flush();
  } catch (...) {
  }
  if (file_) {
    std::fclose(file_);
    file_ = nullptr;
  }
}

void Writer::ensure_open(const std::filesystem::path& path) {
  file_ = std::fopen(path.c_str(), "ab+");
  if (!file_) {
    throw std::runtime_error("failed to open WAL file: " + path.string());
  }
  if (std::fseek(file_, 0, SEEK_END) != 0) {
    throw std::runtime_error("failed to seek WAL file: " + path.string());
  }
  // Recover sequence from existing file if needed.
  std::filesystem::path tmp_path = path;
  Reader reader(path);
  Record record;
  while (reader.next(record)) {
    next_sequence_ = record.header.sequence + 1;
  }
}

void Writer::append(const RecordView& record_view) {
  if (!file_) {
    throw std::runtime_error("WAL writer not open");
  }

  RecordHeader header = record_view.header;
  header.magic = kMagic;
  header.version = 1;
  header.sequence = next_sequence_++;
  header.payload_size = static_cast<std::uint32_t>(record_view.payload.size());
  header.checksum = checksum32(record_view.payload);

  const auto header_bytes = std::as_bytes(std::span(&header, 1));
  buffer_.insert(buffer_.end(), header_bytes.begin(), header_bytes.end());
  const auto payload_bytes = record_view.payload;
  buffer_.insert(buffer_.end(), payload_bytes.begin(), payload_bytes.end());

  if (buffer_.size() >= flush_threshold_) {
    flush();
  }
}

void Writer::flush() {
  if (!file_ || buffer_.empty()) {
    return;
  }

  const auto wrote = std::fwrite(buffer_.data(), 1, buffer_.size(), file_);
  if (wrote != buffer_.size()) {
    throw std::runtime_error("failed to write WAL buffer");
  }
  buffer_.clear();
  std::fflush(file_);
}

void Writer::sync() {
  flush();
  if (!file_) {
    return;
  }
  fsync_file(file_);
}

Reader::Reader(const std::filesystem::path& path)
    : path_(path) {
  file_ = std::fopen(path.c_str(), "rb");
  if (!file_) {
    throw std::runtime_error("failed to open WAL for read: " + path.string());
  }
}

Reader::~Reader() {
  if (file_) {
    std::fclose(file_);
    file_ = nullptr;
  }
}

bool Reader::next(Record& out_record) {
  if (!file_) {
    return false;
  }

  RecordHeader header;
  const auto read_header = std::fread(&header, sizeof(RecordHeader), 1, file_);
  if (read_header != 1) {
    return false;
  }

  if (header.magic != kMagic) {
    throw std::runtime_error("invalid WAL magic");
  }

  out_record.header = header;
  out_record.payload.resize(header.payload_size);
  if (header.payload_size > 0) {
    const auto read_payload = std::fread(out_record.payload.data(), 1, header.payload_size, file_);
    if (read_payload != header.payload_size) {
      throw std::runtime_error("truncated WAL record");
    }

    if (header.checksum != checksum32(std::span<const std::byte>(out_record.payload.data(), out_record.payload.size()))) {
      throw std::runtime_error("WAL checksum mismatch");
    }
  }

  return true;
}

void Reader::seek_sequence(std::uint64_t sequence) {
  if (!file_) {
    return;
  }
  std::rewind(file_);
  Record record;
  while (next(record)) {
    if (record.header.sequence >= sequence) {
      const auto offset = static_cast<long>(sizeof(RecordHeader) + record.header.payload_size);
      if (std::fseek(file_, -offset, SEEK_CUR) != 0) {
        throw std::runtime_error("failed to seek in WAL");
      }
      break;
    }
  }
}

}  // namespace wal
}  // namespace tradecore
