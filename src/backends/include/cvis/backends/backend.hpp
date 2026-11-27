#pragma once

/**
 * @file backend.hpp
 * @brief Backend abstraction: ModelConfig, ModelMetadata, the IInferenceBackend
 *        interface, and the BackendFactory.
 * @ingroup cvis_backends
 */

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

/**
 * @brief Startup description of a model to load (design interfaces §2).
 * @ingroup cvis_backends
 */
struct ModelConfig {
    std::string model_id;
    std::string path;                         ///< filesystem path to ONNX (OQ-3)
    std::string backend;                      ///< "onnxruntime" | "tensorrt" | "fake"
    bool use_gpu = false;                     ///< request CUDA EP (FR-22 fallback)
    std::map<std::string, std::string> options;
};

/**
 * @brief Introspection of a loaded model.
 * @ingroup cvis_backends
 */
struct ModelMetadata {
    std::string model_id;
    std::string backend;
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
    bool ready = false;
};

/**
 * @brief Pure abstraction: one instance owns one loaded model.
 *
 * Adding a backend never touches api/scheduler (design interfaces §2,
 * FR-8/NFR-12).
 * @ingroup cvis_backends
 */
class IInferenceBackend {
public:
    virtual ~IInferenceBackend() = default;

    /**
     * @brief Load/prepare the model. Called once at startup.
     * @param cfg Model description.
     * @return Status describing success or the load failure; never throws.
     */
    virtual core::Status load(const ModelConfig& cfg) = 0;

    /// @brief Introspection of the loaded model (names, readiness).
    [[nodiscard]] virtual const ModelMetadata& metadata() const noexcept = 0;

    /**
     * @brief Execute a coalesced batch.
     *
     * Requests are borrowed (non-owning) and outlive the call.
     * @param batch Index-aligned borrowed request pointers.
     * @return One response per request, index-aligned (FR-18), or a Status on
     *         failure.
     * @note Thread-safety: the scheduler guarantees a single concurrent call
     *       per backend instance (design §6/§9).
     */
    virtual core::Expected<std::vector<core::InferResponse>>
    executeBatch(std::span<const core::InferRequest* const> batch) = 0;
};

/**
 * @brief Factory: backend name -> instance.
 *
 * Impls are compile-time gated (OnnxRuntime under `CVIS_ENABLE_ONNXRUNTIME`,
 * TensorRt under `CVIS_ENABLE_TENSORRT`, Fake always). Registration happens at
 * startup only.
 * @ingroup cvis_backends
 */
class BackendFactory {
public:
    using Creator = std::function<std::unique_ptr<IInferenceBackend>()>;

    /// @brief Register a creator under @p name (last registration wins).
    void registerBackend(std::string name, Creator creator);

    /**
     * @brief Create a backend instance by name.
     * @param name Registered backend name.
     * @return A new backend, or an UNIMPLEMENTED-style error if the backend was
     *         not built/registered (FR-9).
     */
    [[nodiscard]] core::Expected<std::unique_ptr<IInferenceBackend>>
    create(std::string_view name) const;

    /// @brief True if a creator is registered under @p name.
    [[nodiscard]] bool has(std::string_view name) const;

    /**
     * @brief Register every backend compiled into this build.
     *
     * Fake always; OnnxRuntime/TensorRt when their CMake options are enabled.
     */
    void registerBuiltins();

private:
    std::map<std::string, Creator, std::less<>> creators_;
};

} // namespace cvis::backends
