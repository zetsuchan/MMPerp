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

add_library(tradecore::core INTERFACE)
target_include_directories(tradecore::core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:include>)
