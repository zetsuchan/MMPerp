add_library(tradecore_risk STATIC
  src/risk_engine.cpp
)

target_include_directories(tradecore_risk
  PUBLIC
    include
)

target_compile_features(tradecore_risk PUBLIC cxx_std_20)

target_link_libraries(tradecore_risk
  PUBLIC
    tradecore::common
)

add_library(tradecore::risk ALIAS tradecore_risk)
