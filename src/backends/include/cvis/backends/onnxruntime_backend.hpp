#pragma once

/**
 * @file onnxruntime_backend.hpp
 * @brief ONNX Runtime inference backend (CPU EP always; CUDA EP when gated on).
 * @ingroup cvis_backends
 */

// Compiled only under CVIS_ENABLE_ONNXRUNTIME. Declares the ONNX Runtime backend
// (CPU EP always; CUDA EP appended under CVIS_ENABLE_CUDA with transparent CPU
// fallback, design interfaces §2 / FR-22). Header is dependency-light so it can
// be included by the factory without pulling in the ORT headers.

#include <memory>

#include "cvis/backends/backend.hpp"

namespace cvis::backends {

/**
 * @brief ONNX Runtime backend: CPU execution provider always, CUDA EP appended
 *        under `CVIS_ENABLE_CUDA` with transparent CPU fallback (FR-22).
 *
 * The ORT types are hidden behind a PIMPL so including this header does not pull
 * in the ONNX Runtime headers (design interfaces §2).
 * @note Compiled only under `CVIS_ENABLE_ONNXRUNTIME`; excluded from the default
 *       CPU-only build when that option is off.
 * @ingroup cvis_backends
 */
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
