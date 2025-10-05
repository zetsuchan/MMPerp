add_library(tradecore_funding STATIC
  src/funding_engine.cpp
)

target_include_directories(tradecore_funding
  PUBLIC
    include
)

target_compile_features(tradecore_funding PUBLIC cxx_std_20)

target_link_libraries(tradecore_funding
  PUBLIC
    tradecore::common
)

add_library(tradecore::funding ALIAS tradecore_funding)
