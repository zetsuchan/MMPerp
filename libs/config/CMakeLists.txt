add_library(tradecore_config STATIC
  src/config_loader.cpp
)

target_include_directories(tradecore_config
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party
)

add_library(tradecore::config ALIAS tradecore_config)
