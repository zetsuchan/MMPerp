add_executable(tradecore_unit_tests
  main.cpp
)

target_compile_features(tradecore_unit_tests PUBLIC cxx_std_20)

target_link_libraries(tradecore_unit_tests
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

add_test(NAME tradecore_unit_tests COMMAND tradecore_unit_tests)
