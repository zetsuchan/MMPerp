add_library(tradecore_common STATIC
  src/types.cpp
  src/time_utils.cpp
)

target_include_directories(tradecore_common
  PUBLIC
    include
)

target_compile_features(tradecore_common PUBLIC cxx_std_20)

target_link_libraries(tradecore_common
  PUBLIC
    tradecore::core
)

add_library(tradecore::common ALIAS tradecore_common)
