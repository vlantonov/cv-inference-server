#pragma once

/**
 * @file model_registry.hpp
 * @brief Owns loaded backend instances and answers readiness/lookup queries.
 * @ingroup cvis_runtime
 */

#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "cvis/backends/backend.hpp"
#include "cvis/core/status.hpp"
#include "cvis/runtime/config.hpp"

namespace cvis::runtime {

/**
 * @brief Holds loaded backend instances and answers readiness (design
 *        interfaces §6, FR-3/FR-25).
 *
 * Guarded by a shared_mutex to allow future runtime load/unload (OQ-6 deferred).
 * @ingroup cvis_runtime
 */
class ModelRegistry {
public:
    ModelRegistry() = default;

    ModelRegistry(const ModelRegistry&) = delete;
    ModelRegistry& operator=(const ModelRegistry&) = delete;

    /**
     * @brief Create and load every model in @p cfg via @p factory. Startup only.
     * @param cfg     Server configuration listing the models to load.
     * @param factory Backend factory used to instantiate each backend.
     * @return OK on success, or the first load failure's Status.
     */
    core::Status loadFromConfig(const Config& cfg, backends::BackendFactory& factory);

    /// @brief Metadata for every loaded model (FR-3). THREAD-SAFE.
    [[nodiscard]] std::vector<backends::ModelMetadata> list() const;   // FR-3
    /// @brief True when at least one model reports ready (FR-25). THREAD-SAFE.
    [[nodiscard]] bool anyReady() const noexcept;                      // FR-25

    /**
     * @brief Non-owning lookup of a loaded backend by model id.
     * @param model_id Model identifier.
     * @return Backend pointer, or nullptr if unknown (design interfaces §6).
     */
    [[nodiscard]] backends::IInferenceBackend* find(std::string_view model_id) noexcept;

private:
    mutable std::shared_mutex mu_;
    std::map<std::string, std::unique_ptr<backends::IInferenceBackend>, std::less<>> backends_;
};

} // namespace cvis::runtime
