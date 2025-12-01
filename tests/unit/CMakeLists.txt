add_executable(tradecore_unit_tests
  main.cpp
  test_api.cpp
  test_funding.cpp
  test_ingest.cpp
  test_ledger.cpp
  test_matcher.cpp
  test_persistence.cpp
  test_risk.cpp
  test_telemetry.cpp
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
