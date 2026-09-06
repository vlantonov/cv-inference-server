# cvis_runtime: composition root (Config, ModelRegistry, ServerApp). The core
# pieces are dependency-free; YAML/CLI config and the gRPC server are gated so
# the runtime (and cvis_server) build on a bare CPU machine (design §6, NFR-8).
add_library(cvis_runtime STATIC
    src/config.cpp
    src/model_registry.cpp
    src/server_app.cpp
)

target_include_directories(cvis_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(cvis_runtime
    PUBLIC cvis_core cvis_backends cvis_batching cvis_scheduler cvis_metrics
    PRIVATE cvis_warnings cvis_sanitizers
)

if(CVIS_ENABLE_CONFIG_YAML)
    target_compile_definitions(cvis_runtime PRIVATE CVIS_ENABLE_CONFIG_YAML)
    target_link_libraries(cvis_runtime PRIVATE yaml-cpp::yaml-cpp CLI11::CLI11)
endif()

if(CVIS_ENABLE_GRPC)
    target_compile_definitions(cvis_runtime PRIVATE CVIS_ENABLE_GRPC)
    target_link_libraries(cvis_runtime PUBLIC cvis_api)
endif()

add_library(cvis::runtime ALIAS cvis_runtime)
