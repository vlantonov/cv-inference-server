#pragma once

// Compiled only under CVIS_ENABLE_ONNXRUNTIME. Declares the ONNX Runtime backend
// (CPU EP always; CUDA EP appended under CVIS_ENABLE_CUDA with transparent CPU
// fallback, design interfaces §2 / FR-22). Header is dependency-light so it can
// be included by the factory without pulling in the ORT headers.

#include <memory>

#include "cvis/backends/backend.hpp"

namespace cvis::backends {

class OnnxRuntimeBackend final : public IInferenceBackend {
public:
    OnnxRuntimeBackend();
    ~OnnxRuntimeBackend() override;

    OnnxRuntimeBackend(const OnnxRuntimeBackend&) = delete;
    OnnxRuntimeBackend& operator=(const OnnxRuntimeBackend&) = delete;

    core::Status load(const ModelConfig& cfg) override;
    [[nodiscard]] const ModelMetadata& metadata() const noexcept override;
    core::Expected<std::vector<core::InferResponse>>
    executeBatch(std::span<const core::InferRequest* const> batch) override;

private:
    struct Impl;                 // PIMPL hides the ORT types (rebuild churn)
    std::unique_ptr<Impl> impl_;
    ModelMetadata metadata_{};
};

} // namespace cvis::backends
