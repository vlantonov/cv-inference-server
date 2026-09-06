# cvis_backends: IInferenceBackend + factory + FakeBackend always built.
# OnnxRuntimeBackend / TensorRtBackend are gated so the default CPU build never
# needs ONNX Runtime, CUDA, or TensorRT (design §5.2, FR-9).
set(_cvis_backend_sources
    src/backend_factory.cpp
    src/fake_backend.cpp
)

if(CVIS_ENABLE_ONNXRUNTIME)
    list(APPEND _cvis_backend_sources src/onnxruntime_backend.cpp)
endif()

if(CVIS_ENABLE_TENSORRT)
    list(APPEND _cvis_backend_sources src/tensorrt_backend.cpp)
endif()

add_library(cvis_backends STATIC ${_cvis_backend_sources})

target_include_directories(cvis_backends PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(cvis_backends
    PUBLIC cvis_core
    PRIVATE cvis_warnings cvis_sanitizers
)

if(CVIS_ENABLE_ONNXRUNTIME)
    target_compile_definitions(cvis_backends PRIVATE CVIS_ENABLE_ONNXRUNTIME)
    target_link_libraries(cvis_backends PRIVATE onnxruntime::onnxruntime)
    if(CVIS_ENABLE_CUDA)
        target_compile_definitions(cvis_backends PRIVATE CVIS_ENABLE_CUDA)
    endif()
endif()

if(CVIS_ENABLE_TENSORRT)
    target_compile_definitions(cvis_backends PRIVATE CVIS_ENABLE_TENSORRT CVIS_ENABLE_CUDA)
    # find_package(TensorRT) / CUDA toolkit links are added during Phase 4 GPU
    # bring-up; intentionally omitted from the CPU build.
endif()

add_library(cvis::backends ALIAS cvis_backends)
