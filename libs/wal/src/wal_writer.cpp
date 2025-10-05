#include "tradecore/wal/wal_writer.hpp"

namespace tradecore {
namespace wal {

void WalWriter::open(const std::filesystem::path& path) {
  path_ = path;
}

void WalWriter::append(std::span<const std::byte> /*payload*/) {
  // Placeholder: append to write-ahead log using deterministic framing.
}

void WalWriter::flush() {
  // Placeholder: fsync policy to be enforced here.
}

}  // namespace wal
}  // namespace tradecore
