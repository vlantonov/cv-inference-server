# cvis_scheduler: fair dispatch + backpressure. Dependency-free (links only
# core, backends, metrics) so it is fully unit-testable against FakeBackend on a
# CPU-only machine (design §1.2, §2, NFR-10).
find_package(Threads REQUIRED)

add_library(cvis_scheduler STATIC
    src/scheduler.cpp
)

target_include_directories(cvis_scheduler PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(cvis_scheduler
    PUBLIC cvis_core cvis_backends cvis_metrics
    PRIVATE cvis_warnings cvis_sanitizers Threads::Threads
)

find_package(Threads REQUIRED)

add_library(cvis::scheduler ALIAS cvis_scheduler)
