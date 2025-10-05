#include "tradecore/snapshot/snapshot_store.hpp"

namespace tradecore {
namespace snapshot {

void SnapshotStore::prepare(const std::filesystem::path& directory) {
  directory_ = directory;
}

void SnapshotStore::persist(common::SequenceId /*sequence_id*/) {
  // Placeholder: snapshot serialization will stream deterministic state checkpoints.
}

}  // namespace snapshot
}  // namespace tradecore
