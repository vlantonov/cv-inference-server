# cvis_batching: dynamic batcher. Dependency-free (core, scheduler, metrics) so
# it is fully unit-testable against FakeBackend on CPU CI (design §2, §7, NFR-10).
find_package(Threads REQUIRED)

add_library(cvis_batching STATIC
    src/dynamic_batcher.cpp
)

target_include_directories(cvis_batching PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(cvis_batching
    PUBLIC cvis_core cvis_scheduler cvis_metrics
    PRIVATE cvis_warnings cvis_sanitizers Threads::Threads
)

add_library(cvis::batching ALIAS cvis_batching)
