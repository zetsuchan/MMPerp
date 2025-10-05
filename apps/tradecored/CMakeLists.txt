add_executable(tradecored
  src/main.cpp
)

target_compile_features(tradecored PUBLIC cxx_std_20)

target_link_libraries(tradecored
  PRIVATE
    tradecore::ingest
    tradecore::matcher
    tradecore::risk
    tradecore::funding
    tradecore::ledger
    tradecore::wal
    tradecore::snapshot
    tradecore::replay
    tradecore::telemetry
    tradecore::api
)

set_target_properties(tradecored PROPERTIES OUTPUT_NAME "tradecored")
