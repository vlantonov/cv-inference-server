# cvis_core: dependency-free value types. Buildable and unit-testable on any
# CPU-only machine with no third-party libraries (design §1.2, §2).
add_library(cvis_core STATIC
    src/data_type.cpp
    src/status.cpp
    src/tensor.cpp
)

target_include_directories(cvis_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(cvis_core
    PRIVATE cvis_warnings cvis_sanitizers
)

add_library(cvis::core ALIAS cvis_core)
