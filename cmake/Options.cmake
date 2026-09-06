# Build options (design §5.2). ALL GPU/Vulkan options default OFF so a bare
# CPU machine yields a working build + tests (NFR-8).

# --- GPU / Vulkan: opt-in only, never required for the CPU vertical slice ---
option(CVIS_ENABLE_CUDA        "Enable ONNX Runtime CUDA execution provider path"      OFF)
option(CVIS_ENABLE_TENSORRT    "Build TensorRtBackend (implies CVIS_ENABLE_CUDA)"       OFF)
option(CVIS_ENABLE_VULKAN_BENCH "Build the Vulkan benchmark-only compute path"          OFF)

# --- Tooling ---
option(CVIS_ENABLE_SANITIZERS  "Enable ASan/LSan or TSan instrumentation"               OFF)
set(CVIS_SANITIZER "address" CACHE STRING "Sanitizer to use when enabled: address|thread")
set_property(CACHE CVIS_SANITIZER PROPERTY STRINGS address thread)

option(CVIS_BUILD_TESTS "Build unit tests and register them with CTest" ON)
option(CVIS_BUILD_BENCH "Build the cvis_bench benchmark harness"        ON)

# TensorRT implies CUDA (design §5.2).
if(CVIS_ENABLE_TENSORRT AND NOT CVIS_ENABLE_CUDA)
    message(STATUS "CVIS_ENABLE_TENSORRT=ON -> forcing CVIS_ENABLE_CUDA=ON")
    set(CVIS_ENABLE_CUDA ON CACHE BOOL "" FORCE)
endif()
