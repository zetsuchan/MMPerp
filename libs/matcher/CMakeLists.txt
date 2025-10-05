add_library(tradecore_matcher STATIC
  src/matching_engine.cpp
)

target_include_directories(tradecore_matcher
  PUBLIC
    include
)

target_compile_features(tradecore_matcher PUBLIC cxx_std_20)

target_link_libraries(tradecore_matcher
  PUBLIC
    tradecore::common
)

add_library(tradecore::matcher ALIAS tradecore_matcher)
