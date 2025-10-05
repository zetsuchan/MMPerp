add_library(tradecore_ingest STATIC
  src/ingress_pipeline.cpp
)

target_include_directories(tradecore_ingest
  PUBLIC
    include
)

target_compile_features(tradecore_ingest PUBLIC cxx_std_20)

target_link_libraries(tradecore_ingest
  PUBLIC
    tradecore::common
)

add_library(tradecore::ingest ALIAS tradecore_ingest)
