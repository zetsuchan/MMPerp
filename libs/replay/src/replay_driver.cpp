#include "tradecore/replay/replay_driver.hpp"

namespace tradecore {
namespace replay {

void ReplayDriver::load(common::SequenceId from_sequence, common::SequenceId to_sequence) {
  from_ = from_sequence;
  to_ = to_sequence;
}

void ReplayDriver::execute() {
  // Placeholder: deterministic replay harness will consume WAL and snapshots here.
}

}  // namespace replay
}  // namespace tradecore
