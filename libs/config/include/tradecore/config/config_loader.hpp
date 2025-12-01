#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tradecore {
namespace config {

struct TransportConfig {
  std::string endpoint{"quic://127.0.0.1:9000"};
};

struct IngressConfig {
  std::size_t new_order_queue_depth{1 << 12};
  std::size_t cancel_queue_depth{1 << 12};
  std::size_t replace_queue_depth{1 << 12};
  std::uint32_t max_new_orders_per_second{10'000};
  std::uint32_t max_cancels_per_second{20'000};
};

struct MarketRiskConfig {
  std::int64_t contract_size{1};
  std::int32_t initial_margin_basis_points{500};
  std::int32_t maintenance_margin_basis_points{300};
  std::int64_t initial_mark_price{1000};
};

struct MarketFundingConfig {
  std::int32_t clamp_basis_points{50};
  std::int64_t max_rate_basis_points{100};
};

struct MarketConfig {
  std::uint32_t id{1};
  std::string symbol{"BTC-PERP"};
  MarketRiskConfig risk;
  MarketFundingConfig funding;
};

struct MatcherConfig {
  std::size_t arena_bytes{1 << 20};
};

struct PersistenceConfig {
  std::filesystem::path wal_path{"/var/lib/tradecore/events.wal"};
  std::filesystem::path snapshot_dir{"/var/lib/tradecore/snapshots"};
  std::size_t wal_flush_threshold{128};
};

struct TelemetryConfig {
  bool enabled{true};
  std::size_t buffer_size{1024};
};

struct EngineConfig {
  TransportConfig transport;
  IngressConfig ingress;
  MatcherConfig matcher;
  PersistenceConfig persistence;
  TelemetryConfig telemetry;
  std::vector<MarketConfig> markets;
};

struct ValidationError {
  std::string field;
  std::string message;
};

struct LoadResult {
  bool success{false};
  EngineConfig config;
  std::vector<ValidationError> errors;
  std::string raw_error;
};

class ConfigLoader {
 public:
  static LoadResult load(const std::filesystem::path& path);
  static LoadResult load_from_string(std::string_view toml_content);
  static std::vector<ValidationError> validate(const EngineConfig& config);
  static std::string generate_default();
};

}  // namespace config
}  // namespace tradecore
