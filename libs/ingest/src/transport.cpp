#include "tradecore/ingest/transport.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <regex>
#include <stdexcept>

namespace tradecore {
namespace ingest {

namespace {

struct EndpointInfo {
  std::string host;
  std::uint16_t port;
  bool valid;
};

EndpointInfo parse_endpoint(const std::string& uri) {
  // Parse "quic://host:port" or "udp://host:port"
  std::regex pattern(R"((quic|udp)://([^:]+):(\d+))");
  std::smatch match;

  if (std::regex_match(uri, match, pattern)) {
    return {match[2].str(), static_cast<std::uint16_t>(std::stoi(match[3].str())), true};
  }
  return {"", 0, false};
}

}  // namespace

UdpTransport::UdpTransport() = default;

UdpTransport::~UdpTransport() {
  stop();
}

bool UdpTransport::start(const std::string& endpoint_uri, FrameCallback callback) {
  if (running_.load()) {
    return false;
  }

  auto endpoint = parse_endpoint(endpoint_uri);
  if (!endpoint.valid) {
    return false;
  }

  callback_ = std::move(callback);

  // Create UDP socket
  socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_fd_ < 0) {
    return false;
  }

  // Allow address reuse
  int opt = 1;
  setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // Bind to endpoint
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(endpoint.port);

  if (endpoint.host == "0.0.0.0" || endpoint.host.empty()) {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    if (inet_pton(AF_INET, endpoint.host.c_str(), &addr.sin_addr) != 1) {
      close(socket_fd_);
      socket_fd_ = -1;
      return false;
    }
  }

  if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(socket_fd_);
    socket_fd_ = -1;
    return false;
  }

  // Set receive timeout for clean shutdown
  timeval tv{};
  tv.tv_sec = 0;
  tv.tv_usec = 100000;  // 100ms
  setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  running_.store(true);
  receive_thread_ = std::thread(&UdpTransport::receive_loop, this);

  return true;
}

void UdpTransport::stop() {
  running_.store(false);

  if (receive_thread_.joinable()) {
    receive_thread_.join();
  }

  if (socket_fd_ >= 0) {
    close(socket_fd_);
    socket_fd_ = -1;
  }

  callback_ = nullptr;
}

bool UdpTransport::is_running() const {
  return running_.load();
}

TransportStats UdpTransport::stats() const {
  return {
      .bytes_received = bytes_received_.load(),
      .frames_received = frames_received_.load(),
      .frames_malformed = frames_malformed_.load(),
      .connections_active = running_.load() ? 1UL : 0UL,
  };
}

void UdpTransport::receive_loop() {
  constexpr std::size_t kMaxDatagramSize = 65536;
  std::array<std::byte, kMaxDatagramSize> buffer{};
  std::vector<std::byte> payload_storage;
  payload_storage.reserve(4096);

  while (running_.load()) {
    sockaddr_in sender_addr{};
    socklen_t sender_len = sizeof(sender_addr);

    ssize_t received = recvfrom(
        socket_fd_,
        buffer.data(),
        buffer.size(),
        0,
        reinterpret_cast<sockaddr*>(&sender_addr),
        &sender_len);

    if (received < 0) {
      // Timeout or error, check if still running
      continue;
    }

    if (received == 0) {
      continue;
    }

    bytes_received_.fetch_add(static_cast<std::uint64_t>(received));

    Frame frame;
    if (parse_frame(buffer.data(), static_cast<std::size_t>(received), frame, payload_storage)) {
      frames_received_.fetch_add(1);
      if (callback_) {
        callback_(frame);
      }
    } else {
      frames_malformed_.fetch_add(1);
    }
  }
}

bool UdpTransport::parse_frame(const std::byte* data, std::size_t len, Frame& out_frame, std::vector<std::byte>& payload_storage) {
  if (len < sizeof(WireHeader)) {
    return false;
  }

  const auto* header = reinterpret_cast<const WireHeader*>(data);

  // Validate magic and version
  if (header->magic != WireHeader::kMagic) {
    return false;
  }

  if (header->version != WireHeader::kVersion) {
    return false;
  }

  // Validate payload length
  std::size_t expected_len = sizeof(WireHeader) + header->payload_len;
  if (len < expected_len) {
    return false;
  }

  // Validate message kind
  if (header->kind > static_cast<std::uint8_t>(MessageKind::kHeartbeat)) {
    return false;
  }

  // Build frame header
  out_frame.header.account = header->account;
  out_frame.header.nonce = header->nonce;
  out_frame.header.received_time_ns = header->timestamp_ns;
  out_frame.header.priority = header->priority;
  out_frame.header.kind = static_cast<MessageKind>(header->kind);

  // Copy payload
  if (header->payload_len > 0) {
    const std::byte* payload_start = data + sizeof(WireHeader);
    payload_storage.assign(payload_start, payload_start + header->payload_len);
    out_frame.payload = std::span<const std::byte>(payload_storage);
  } else {
    payload_storage.clear();
    out_frame.payload = std::span<const std::byte>();
  }

  return true;
}

}  // namespace ingest
}  // namespace tradecore
