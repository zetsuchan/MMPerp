#include "tradecore/ingest/quic_transport.hpp"

namespace tradecore {
namespace ingest {

void QuicTransport::start(std::string endpoint_uri, FrameCallback callback) {
  endpoint_ = std::move(endpoint_uri);
  callback_ = std::move(callback);
  // Real implementation would start QUIC listener and feed frames into callback.
}

void QuicTransport::stop() {
  callback_ = nullptr;
  endpoint_.clear();
}

}  // namespace ingest
}  // namespace tradecore
