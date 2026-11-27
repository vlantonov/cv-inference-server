#pragma once

/**
 * @file tensorrt_backend.hpp
 * @brief TensorRT inference backend (ONNX->TRT with an on-disk engine cache).
 * @ingroup cvis_backends
 */

// Compiled only under CVIS_ENABLE_TENSORRT (implies CVIS_ENABLE_CUDA). ONNX->TRT
// build with an on-disk engine cache keyed by model hash + TRT version + device
// (design interfaces §2, OQ-3, R-4). Not part of the default CPU build.

#include <memory>

#include "cvis/backends/backend.hpp"

namespace cvis::backends {

/**
 * @brief TensorRT backend: builds an engine from ONNX and caches it on disk,
 *        keyed by model hash + TRT version + device (OQ-3, R-4).
 *
 * The TRT/CUDA types are hidden behind a PIMPL to keep this header dependency-
 * light.
 * @note Compiled only under `CVIS_ENABLE_TENSORRT` (implies `CVIS_ENABLE_CUDA`);
 *       not part of the default CPU build.
 * @ingroup cvis_backends
 */
class TensorRtBackend final : public IInferenceBackend {
public:
    TensorRtBackend();
    ~TensorRtBackend() override;

    TensorRtBackend(const TensorRtBackend&) = delete;
    TensorRtBackend& operator=(const TensorRtBackend&) = delete;

    core::Status load(const ModelConfig& cfg) override;
    [[nodiscard]] const ModelMetadata& metadata() const noexcept override;
    core::Expected<std::vector<core::InferResponse>>
    executeBatch(std::span<const core::InferRequest* const> batch) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ModelMetadata metadata_{};
};

} // namespace cvis::backends
