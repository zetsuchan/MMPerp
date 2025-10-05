add_library(tradecore_ledger STATIC
  src/ledger_state.cpp
)

target_include_directories(tradecore_ledger
  PUBLIC
    include
)

target_compile_features(tradecore_ledger PUBLIC cxx_std_20)

target_link_libraries(tradecore_ledger
  PUBLIC
    tradecore::common
)

add_library(tradecore::ledger ALIAS tradecore_ledger)
