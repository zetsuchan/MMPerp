cmake_minimum_required(VERSION 3.23)

project(tradecore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TRADECORE_ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)
option(TRADECORE_BUILD_TESTS "Build TradeCore unit and integration tests" ON)

if(TRADECORE_ENABLE_SANITIZERS)
  set(SANITIZER_FLAGS "-fsanitize=address,undefined")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
  set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()

add_subdirectory(libs)
add_subdirectory(apps)

if(TRADECORE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
