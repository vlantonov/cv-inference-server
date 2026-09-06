cmake_minimum_required(VERSION 3.24)

project(cv-inference-server
    VERSION 0.1.0
    DESCRIPTION "GPU-accelerated CV inference server (portfolio)"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Language / global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Options)      # CVIS_* build options (all GPU/Vulkan default OFF)
include(Warnings)     # cvis_warnings interface target
include(Sanitizers)   # cvis_sanitizers interface target

# ---------------------------------------------------------------------------
# Optional heavy dependencies -- auto-detected, gracefully degrading.
# The dependency-free core (cvis_core, FakeBackend, batching, scheduler,
# metrics) NEVER needs any of these. Each is gated so a bare CPU machine
# still produces a green build + tests (design §5.2 / §12, NFR-8/NFR-10).
# ---------------------------------------------------------------------------
include(Dependencies)

# ---------------------------------------------------------------------------
# Core library layers (dependency-free, always built)
# ---------------------------------------------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/metrics)
add_subdirectory(src/backends)
add_subdirectory(src/scheduler)
add_subdirectory(src/batching)
add_subdirectory(src/runtime)

# gRPC transport layer -- only when protobuf+gRPC toolchain is available.
if(CVIS_ENABLE_GRPC)
    add_subdirectory(src/api)
endif()

# ---------------------------------------------------------------------------
# Applications
# ---------------------------------------------------------------------------
add_subdirectory(apps/server)

if(CVIS_BUILD_BENCH)
    add_subdirectory(apps/bench)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(CVIS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Configuration summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "cv-inference-server ${PROJECT_VERSION} configuration:")
message(STATUS "  Build type ............ ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard .......... ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests (CVIS_BUILD_TESTS) . ${CVIS_BUILD_TESTS}")
message(STATUS "  Bench (CVIS_BUILD_BENCH) . ${CVIS_BUILD_BENCH}")
message(STATUS "  Sanitizers ............ ${CVIS_ENABLE_SANITIZERS} (${CVIS_SANITIZER})")
message(STATUS "  -- optional / gated deps --")
message(STATUS "  gRPC transport ........ ${CVIS_ENABLE_GRPC}")
message(STATUS "  ONNX Runtime backend .. ${CVIS_ENABLE_ONNXRUNTIME}")
message(STATUS "  Metrics HTTP exposer .. ${CVIS_ENABLE_METRICS_HTTP}")
message(STATUS "  YAML/CLI config ....... ${CVIS_ENABLE_CONFIG_YAML}")
message(STATUS "  -- GPU / Vulkan (opt-in, OFF by default) --")
message(STATUS "  CUDA (ORT CUDA EP) .... ${CVIS_ENABLE_CUDA}")
message(STATUS "  TensorRT backend ...... ${CVIS_ENABLE_TENSORRT}")
message(STATUS "  Vulkan bench path ..... ${CVIS_ENABLE_VULKAN_BENCH}")
message(STATUS "")
