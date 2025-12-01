#include "tradecore/config/config_loader.hpp"

#define TOML_EXCEPTIONS 0
#include "toml.hpp"

#include <fstream>
#include <sstream>

namespace tradecore {
namespace config {

namespace {

template <typename T>
T get_or(const toml::table& tbl, std::string_view key, T default_val) {
  if (auto val = tbl[key].value<T>()) {
    return *val;
  }
  return default_val;
}

std::int64_t get_int_or(const toml::table& tbl, std::string_view key, std::int64_t default_val) {
  if (auto val = tbl[key].value<std::int64_t>()) {
    return *val;
  }
  return default_val;
}

std::string get_str_or(const toml::table& tbl, std::string_view key, std::string_view default_val) {
  if (auto val = tbl[key].value<std::string_view>()) {
    return std::string(*val);
  }
  return std::string(default_val);
}

TransportConfig parse_transport(const toml::table& root) {
  TransportConfig cfg;
  if (auto* transport = root["transport"].as_table()) {
    cfg.endpoint = get_str_or(*transport, "endpoint", cfg.endpoint);
  }
  return cfg;
}

IngressConfig parse_ingress(const toml::table& root) {
  IngressConfig cfg;
  if (auto* ingress = root["ingress"].as_table()) {
    cfg.new_order_queue_depth = static_cast<std::size_t>(get_int_or(*ingress, "new_order_queue_depth", cfg.new_order_queue_depth));
    cfg.cancel_queue_depth = static_cast<std::size_t>(get_int_or(*ingress, "cancel_queue_depth", cfg.cancel_queue_depth));
    cfg.replace_queue_depth = static_cast<std::size_t>(get_int_or(*ingress, "replace_queue_depth", cfg.replace_queue_depth));
    cfg.max_new_orders_per_second = static_cast<std::uint32_t>(get_int_or(*ingress, "max_new_orders_per_second", cfg.max_new_orders_per_second));
    cfg.max_cancels_per_second = static_cast<std::uint32_t>(get_int_or(*ingress, "max_cancels_per_second", cfg.max_cancels_per_second));
  }
  return cfg;
}

MatcherConfig parse_matcher(const toml::table& root) {
  MatcherConfig cfg;
  if (auto* matcher = root["matcher"].as_table()) {
    cfg.arena_bytes = static_cast<std::size_t>(get_int_or(*matcher, "arena_bytes", cfg.arena_bytes));
  }
  return cfg;
}

PersistenceConfig parse_persistence(const toml::table& root) {
  PersistenceConfig cfg;
  if (auto* persistence = root["persistence"].as_table()) {
    cfg.wal_path = get_str_or(*persistence, "wal_path", cfg.wal_path.string());
    cfg.snapshot_dir = get_str_or(*persistence, "snapshot_dir", cfg.snapshot_dir.string());
    cfg.wal_flush_threshold = static_cast<std::size_t>(get_int_or(*persistence, "wal_flush_threshold", cfg.wal_flush_threshold));
  }
  return cfg;
}

TelemetryConfig parse_telemetry(const toml::table& root) {
  TelemetryConfig cfg;
  if (auto* telemetry = root["telemetry"].as_table()) {
    if (auto val = (*telemetry)["enabled"].value<bool>()) {
      cfg.enabled = *val;
    }
    cfg.buffer_size = static_cast<std::size_t>(get_int_or(*telemetry, "buffer_size", cfg.buffer_size));
  }
  return cfg;
}

std::vector<MarketConfig> parse_markets(const toml::table& root) {
  std::vector<MarketConfig> markets;
  if (auto* arr = root["markets"].as_array()) {
    for (const auto& elem : *arr) {
      if (auto* market_tbl = elem.as_table()) {
        MarketConfig market;
        market.id = static_cast<std::uint32_t>(get_int_or(*market_tbl, "id", market.id));
        market.symbol = get_str_or(*market_tbl, "symbol", market.symbol);

        if (auto* risk_tbl = (*market_tbl)["risk"].as_table()) {
          market.risk.contract_size = get_int_or(*risk_tbl, "contract_size", market.risk.contract_size);
          market.risk.initial_margin_basis_points = static_cast<std::int32_t>(get_int_or(*risk_tbl, "initial_margin_bp", market.risk.initial_margin_basis_points));
          market.risk.maintenance_margin_basis_points = static_cast<std::int32_t>(get_int_or(*risk_tbl, "maintenance_margin_bp", market.risk.maintenance_margin_basis_points));
          market.risk.initial_mark_price = get_int_or(*risk_tbl, "initial_mark_price", market.risk.initial_mark_price);
        }

        if (auto* funding_tbl = (*market_tbl)["funding"].as_table()) {
          market.funding.clamp_basis_points = static_cast<std::int32_t>(get_int_or(*funding_tbl, "clamp_bp", market.funding.clamp_basis_points));
          market.funding.max_rate_basis_points = get_int_or(*funding_tbl, "max_rate_bp", market.funding.max_rate_basis_points);
        }

        markets.push_back(std::move(market));
      }
    }
  }

  if (markets.empty()) {
    markets.push_back(MarketConfig{});
  }

  return markets;
}

EngineConfig parse_config(const toml::table& root) {
  EngineConfig cfg;
  cfg.transport = parse_transport(root);
  cfg.ingress = parse_ingress(root);
  cfg.matcher = parse_matcher(root);
  cfg.persistence = parse_persistence(root);
  cfg.telemetry = parse_telemetry(root);
  cfg.markets = parse_markets(root);
  return cfg;
}

}  // namespace

LoadResult ConfigLoader::load(const std::filesystem::path& path) {
  LoadResult result;

  if (!std::filesystem::exists(path)) {
    result.raw_error = "Config file not found: " + path.string();
    return result;
  }

  auto parse_result = toml::parse_file(path.string());
  if (!parse_result) {
    std::ostringstream oss;
    oss << parse_result.error();
    result.raw_error = oss.str();
    return result;
  }

  result.config = parse_config(parse_result.table());
  result.errors = validate(result.config);
  result.success = result.errors.empty();
  return result;
}

LoadResult ConfigLoader::load_from_string(std::string_view toml_content) {
  LoadResult result;

  auto parse_result = toml::parse(toml_content);
  if (!parse_result) {
    std::ostringstream oss;
    oss << parse_result.error();
    result.raw_error = oss.str();
    return result;
  }

  result.config = parse_config(parse_result.table());
  result.errors = validate(result.config);
  result.success = result.errors.empty();
  return result;
}

std::vector<ValidationError> ConfigLoader::validate(const EngineConfig& config) {
  std::vector<ValidationError> errors;

  if (config.transport.endpoint.empty()) {
    errors.push_back({"transport.endpoint", "endpoint cannot be empty"});
  }

  if (config.ingress.max_new_orders_per_second == 0) {
    errors.push_back({"ingress.max_new_orders_per_second", "must be greater than 0"});
  }

  if (config.matcher.arena_bytes < (1 << 16)) {
    errors.push_back({"matcher.arena_bytes", "must be at least 64KB"});
  }

  if (config.persistence.wal_path.empty()) {
    errors.push_back({"persistence.wal_path", "wal_path cannot be empty"});
  }

  if (config.persistence.snapshot_dir.empty()) {
    errors.push_back({"persistence.snapshot_dir", "snapshot_dir cannot be empty"});
  }

  for (std::size_t i = 0; i < config.markets.size(); ++i) {
    const auto& market = config.markets[i];
    std::string prefix = "markets[" + std::to_string(i) + "]";

    if (market.id == 0) {
      errors.push_back({prefix + ".id", "market id must be greater than 0"});
    }

    if (market.risk.contract_size <= 0) {
      errors.push_back({prefix + ".risk.contract_size", "must be positive"});
    }

    if (market.risk.initial_margin_basis_points <= 0) {
      errors.push_back({prefix + ".risk.initial_margin_bp", "must be positive"});
    }

    if (market.risk.maintenance_margin_basis_points <= 0) {
      errors.push_back({prefix + ".risk.maintenance_margin_bp", "must be positive"});
    }

    if (market.risk.maintenance_margin_basis_points > market.risk.initial_margin_basis_points) {
      errors.push_back({prefix + ".risk", "maintenance_margin_bp must be <= initial_margin_bp"});
    }

    if (market.funding.max_rate_basis_points <= 0) {
      errors.push_back({prefix + ".funding.max_rate_bp", "must be positive"});
    }
  }

  return errors;
}

std::string ConfigLoader::generate_default() {
  return R"(# TradeCore Engine Configuration
# Generated default configuration

[transport]
endpoint = "quic://127.0.0.1:9000"

[ingress]
new_order_queue_depth = 4096
cancel_queue_depth = 4096
replace_queue_depth = 4096
max_new_orders_per_second = 100000
max_cancels_per_second = 200000

[matcher]
arena_bytes = 1048576  # 1MB

[persistence]
wal_path = "/var/lib/tradecore/events.wal"
snapshot_dir = "/var/lib/tradecore/snapshots"
wal_flush_threshold = 128

[telemetry]
enabled = true
buffer_size = 1024

[[markets]]
id = 1
symbol = "BTC-PERP"

[markets.risk]
contract_size = 1
initial_margin_bp = 500      # 5%
maintenance_margin_bp = 300  # 3%
initial_mark_price = 100000  # $100,000

[markets.funding]
clamp_bp = 50   # 0.5%
max_rate_bp = 100  # 1%
)";
}

}  // namespace config
}  // namespace tradecore
