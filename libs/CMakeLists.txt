add_library(tradecore_core INTERFACE)
add_library(tradecore::core ALIAS tradecore_core)
target_include_directories(tradecore_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:include>)

add_subdirectory(common)
add_subdirectory(ingest)
add_subdirectory(matcher)
add_subdirectory(risk)
add_subdirectory(funding)
add_subdirectory(ledger)
add_subdirectory(wal)
add_subdirectory(snapshot)
add_subdirectory(replay)
add_subdirectory(telemetry)
add_subdirectory(api)
