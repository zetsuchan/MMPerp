#pragma once

#include <functional>
#include <memory>
#include <string>

#include "tradecore/ingest/frame.hpp"
#include "tradecore/ingest/transport.hpp"

namespace tradecore {
namespace ingest {

// QuicTransport provides the order ingestion interface.
// Currently implemented using UDP transport with QUIC wire protocol compatibility.
// Future: Will upgrade to full QUIC using MsQuic for 0-RTT, multiplexing, etc.
class QuicTransport {
 public:
  using FrameCallback = std::function<void(const Frame&)>;

  QuicTransport();
  ~QuicTransport();

  // Start listening on endpoint (e.g., "quic://127.0.0.1:9000")
  bool start(std::string endpoint_uri, FrameCallback callback);

  // Stop the transport
  void stop();

  // Check if transport is running
  bool is_running() const;

  // Get transport statistics
  TransportStats stats() const;

 private:
  std::unique_ptr<Transport> transport_;
  std::string endpoint_;
};

}  // namespace ingest
}  // namespace tradecore
