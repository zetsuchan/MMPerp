add_library(tradecore_replay STATIC
  src/replay_driver.cpp
)

target_include_directories(tradecore_replay
  PUBLIC
    include
)

target_compile_features(tradecore_replay PUBLIC cxx_std_20)

target_link_libraries(tradecore_replay
  PUBLIC
    tradecore::common
    tradecore::snapshot
    tradecore::wal
)

add_library(tradecore::replay ALIAS tradecore_replay)
