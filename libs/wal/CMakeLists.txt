add_library(tradecore_wal STATIC
  src/wal_writer.cpp
)

target_include_directories(tradecore_wal
  PUBLIC
    include
)

target_compile_features(tradecore_wal PUBLIC cxx_std_20)

target_link_libraries(tradecore_wal
  PUBLIC
    tradecore::common
)

add_library(tradecore::wal ALIAS tradecore_wal)
