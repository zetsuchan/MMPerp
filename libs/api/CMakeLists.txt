add_library(tradecore_api STATIC
  src/api_router.cpp
)

target_include_directories(tradecore_api
  PUBLIC
    include
)

target_compile_features(tradecore_api PUBLIC cxx_std_20)

target_link_libraries(tradecore_api
  PUBLIC
    tradecore::common
)

add_library(tradecore::api ALIAS tradecore_api)
