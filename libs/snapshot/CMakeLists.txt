add_library(tradecore_snapshot STATIC
  src/snapshot_store.cpp
)

target_include_directories(tradecore_snapshot
  PUBLIC
    include
)

target_compile_features(tradecore_snapshot PUBLIC cxx_std_20)

target_link_libraries(tradecore_snapshot
  PUBLIC
    tradecore::common
)

add_library(tradecore::snapshot ALIAS tradecore_snapshot)
