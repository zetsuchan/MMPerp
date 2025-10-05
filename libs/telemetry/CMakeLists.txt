add_library(tradecore_telemetry STATIC
  src/telemetry_sink.cpp
)

target_include_directories(tradecore_telemetry
  PUBLIC
    include
)

target_compile_features(tradecore_telemetry PUBLIC cxx_std_20)

target_link_libraries(tradecore_telemetry
  PUBLIC
    tradecore::common
)

add_library(tradecore::telemetry ALIAS tradecore_telemetry)
