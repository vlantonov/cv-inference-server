// Compiled only under CVIS_ENABLE_TENSORRT (guarded in CMakeLists.txt). This is
// GPU-hardware code: it is structurally complete but only manually verifiable on
// a machine with TensorRT + a CUDA device (design R-1). It never affects the CPU
// build. The heavy TRT includes live behind the option so the header stays light.
#include "cvis/backends/tensorrt_backend.hpp"

#include <filesystem>
#include <fstream>
#include <functional>

namespace cvis::backends {

struct TensorRtBackend::Impl {
    std::string engine_cache_dir;
    // On real hardware this holds nvinfer1::ICudaEngine / IExecutionContext and
    // the CUDA stream. Kept opaque here so the CPU toolchain never needs the TRT
    // SDK headers to parse this translation unit's declarations.
};

TensorRtBackend::TensorRtBackend() : impl_(std::make_unique<Impl>()) {}
TensorRtBackend::~TensorRtBackend() = default;

core::Status TensorRtBackend::load(const ModelConfig& cfg) {
    // Engine-cache key = model hash + TRT version + device (OQ-3, R-4). The
    // full ONNX->TRT build path is completed during Phase 4 GPU bring-up; the
    // structure and cache contract are fixed here.
    auto it = cfg.options.find("engine_cache_dir");
    impl_->engine_cache_dir = (it != cfg.options.end()) ? it->second : "./trt-cache";

    metadata_.model_id = cfg.model_id;
    metadata_.backend = "tensorrt";
    metadata_.ready = false;
    return core::Status::Error(
        core::StatusCode::kUnimplemented,
        "TensorRtBackend requires GPU hardware bring-up (Phase 4, R-1)");
}

const ModelMetadata& TensorRtBackend::metadata() const noexcept { return metadata_; }

core::Expected<std::vector<core::InferResponse>>
TensorRtBackend::executeBatch(std::span<const core::InferRequest* const> /*batch*/) {
    return core::Status::Error(
        core::StatusCode::kUnimplemented,
        "TensorRtBackend execute requires GPU hardware (Phase 4, R-1)");
}

} // namespace cvis::backends
