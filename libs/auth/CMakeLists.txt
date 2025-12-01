find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

add_library(tradecore_auth STATIC
  src/authenticator.cpp
)

target_include_directories(tradecore_auth
  PUBLIC
    include
)

target_link_libraries(tradecore_auth
  PUBLIC
    tradecore::common
    PkgConfig::SODIUM
)

target_compile_features(tradecore_auth PUBLIC cxx_std_20)

add_library(tradecore::auth ALIAS tradecore_auth)
