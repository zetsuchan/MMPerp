#pragma once

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace replay {

class ReplayDriver {
 public:
  void load(common::SequenceId from_sequence, common::SequenceId to_sequence);
  void execute();

 private:
  common::SequenceId from_{0};
  common::SequenceId to_{0};
};

}  // namespace replay
}  // namespace tradecore
