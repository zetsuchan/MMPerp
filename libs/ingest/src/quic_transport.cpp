#include "tradecore/ingest/quic_transport.hpp"

namespace tradecore {
namespace ingest {

QuicTransport::QuicTransport() : transport_(std::make_unique<UdpTransport>()) {}

QuicTransport::~QuicTransport() {
  stop();
}

bool QuicTransport::start(std::string endpoint_uri, FrameCallback callback) {
  endpoint_ = std::move(endpoint_uri);
  return transport_->start(endpoint_, std::move(callback));
}

void QuicTransport::stop() {
  if (transport_) {
    transport_->stop();
  }
  endpoint_.clear();
}

bool QuicTransport::is_running() const {
  return transport_ && transport_->is_running();
}

TransportStats QuicTransport::stats() const {
  if (transport_) {
    return transport_->stats();
  }
  return {};
}

}  // namespace ingest
}  // namespace tradecore
