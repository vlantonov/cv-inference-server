# cvis_metrics: thread-safe metrics façade + optional HTTP /metrics exposer.
# The façade itself is dependency-free so batching/scheduler tests need no
# third-party libs. The HTTP exposer links prometheus-cpp only when available.
add_library(cvis_metrics STATIC
    src/metrics_registry.cpp
    src/metrics_http_exposer.cpp
)

target_include_directories(cvis_metrics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(cvis_metrics
    PUBLIC cvis_core
    PRIVATE cvis_warnings cvis_sanitizers
)

if(CVIS_ENABLE_METRICS_HTTP)
    target_compile_definitions(cvis_metrics PRIVATE CVIS_ENABLE_METRICS_HTTP)
    target_link_libraries(cvis_metrics PRIVATE prometheus-cpp::pull prometheus-cpp::core)
endif()

add_library(cvis::metrics ALIAS cvis_metrics)
