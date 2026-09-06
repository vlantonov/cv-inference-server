#pragma once

#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cvis/core/expected.hpp"
#include "cvis/core/infer.hpp"
#include "cvis/core/status.hpp"

namespace cvis::backends {

/// Startup description of a model to load (design interfaces §2).
struct ModelConfig {
    std::string model_id;
    std::string path;                         ///< filesystem path to ONNX (OQ-3)
    std::string backend;                      ///< "onnxruntime" | "tensorrt" | "fake"
    bool use_gpu = false;                     ///< request CUDA EP (FR-22 fallback)
    std::map<std::string, std::string> options;
};

/// Introspection of a loaded model.
struct ModelMetadata {
    std::string model_id;
    std::string backend;
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
    bool ready = false;
};

/// Pure abstraction: one instance owns one loaded model. Adding a backend never
/// touches api/scheduler (design interfaces §2, FR-8/NFR-12).
class IInferenceBackend {
public:
    virtual ~IInferenceBackend() = default;

    /// Load/prepare the model. Called once at startup; fallible, never throws.
    virtual core::Status load(const ModelConfig& cfg) = 0;

    [[nodiscard]] virtual const ModelMetadata& metadata() const noexcept = 0;

    /// Execute a coalesced batch. Requests are borrowed (non-owning) and outlive
    /// the call. Returns one response per request, index-aligned (FR-18).
    /// Thread-safety: the scheduler guarantees a single concurrent call per
    /// backend instance (design §6/§9).
    virtual core::Expected<std::vector<core::InferResponse>>
    executeBatch(std::span<const core::InferRequest* const> batch) = 0;
};

/// Factory: backend name -> instance. Impls are compile-time gated
/// (OnnxRuntime under CVIS_ENABLE_ONNXRUNTIME, TensorRt under
/// CVIS_ENABLE_TENSORRT, Fake always). Registration happens at startup only.
class BackendFactory {
public:
    using Creator = std::function<std::unique_ptr<IInferenceBackend>()>;

    void registerBackend(std::string name, Creator creator);

    /// UNIMPLEMENTED-style error if the backend was not built/registered (FR-9).
    [[nodiscard]] core::Expected<std::unique_ptr<IInferenceBackend>>
    create(std::string_view name) const;

    [[nodiscard]] bool has(std::string_view name) const;

    /// Registers every backend that was compiled into this build (Fake always,
    /// OnnxRuntime/TensorRt when their CMake options are enabled).
    void registerBuiltins();

private:
    std::map<std::string, Creator, std::less<>> creators_;
};

} // namespace cvis::backends
