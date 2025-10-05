#include "tradecore/ingest/ingress_pipeline.hpp"

namespace tradecore {
namespace ingest {

void IngressPipeline::configure(std::string_view endpoint_uri) {
  endpoint_ = endpoint_uri;
}

bool IngressPipeline::enqueue(FrameView /*frame*/) {
  // Placeholder: The real implementation will validate, classify, and push frames onto SPSC rings.
  return true;
}

void IngressPipeline::drain() {
  // Placeholder: drain queued frames into the deterministic writer thread.
}

}  // namespace ingest
}  // namespace tradecore
