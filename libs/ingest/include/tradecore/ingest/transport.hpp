#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "tradecore/ingest/frame.hpp"

namespace tradecore {
namespace ingest {

struct TransportStats {
  std::uint64_t bytes_received{0};
  std::uint64_t frames_received{0};
  std::uint64_t frames_malformed{0};
  std::uint64_t connections_active{0};
};

class Transport {
 public:
  using FrameCallback = std::function<void(const Frame&)>;

  virtual ~Transport() = default;

  virtual bool start(const std::string& endpoint_uri, FrameCallback callback) = 0;
  virtual void stop() = 0;
  virtual bool is_running() const = 0;
  virtual TransportStats stats() const = 0;
};

// Wire protocol for frames over UDP/QUIC
// Header: [magic:4][version:2][flags:2][account:8][nonce:8][timestamp:8][priority:1][kind:1][payload_len:2]
// Total header: 36 bytes, followed by payload
struct WireHeader {
  static constexpr std::uint32_t kMagic = 0x54524443;  // "TRDC"
  static constexpr std::uint16_t kVersion = 1;

  std::uint32_t magic;
  std::uint16_t version;
  std::uint16_t flags;
  std::uint64_t account;
  std::uint64_t nonce;
  std::uint64_t timestamp_ns;
  std::uint8_t priority;
  std::uint8_t kind;
  std::uint16_t payload_len;
} __attribute__((packed));

static_assert(sizeof(WireHeader) == 36, "WireHeader must be 36 bytes");

class UdpTransport : public Transport {
 public:
  UdpTransport();
  ~UdpTransport() override;

  bool start(const std::string& endpoint_uri, FrameCallback callback) override;
  void stop() override;
  bool is_running() const override;
  TransportStats stats() const override;

 private:
  void receive_loop();
  bool parse_frame(const std::byte* data, std::size_t len, Frame& out_frame, std::vector<std::byte>& payload_storage);

  FrameCallback callback_;
  std::atomic<bool> running_{false};
  std::thread receive_thread_;
  int socket_fd_{-1};

  mutable std::atomic<std::uint64_t> bytes_received_{0};
  mutable std::atomic<std::uint64_t> frames_received_{0};
  mutable std::atomic<std::uint64_t> frames_malformed_{0};
};

}  // namespace ingest
}  // namespace tradecore
