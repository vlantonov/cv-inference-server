#pragma once

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

/// Holds loaded backend instances and answers readiness (design interfaces §6,
/// FR-3/FR-25). Guarded by a shared_mutex to allow future runtime load/unload
/// (OQ-6 deferred).
class ModelRegistry {
public:
    ModelRegistry() = default;

    ModelRegistry(const ModelRegistry&) = delete;
    ModelRegistry& operator=(const ModelRegistry&) = delete;

    /// Create + load every model in \p cfg via \p factory. Startup only.
    core::Status loadFromConfig(const Config& cfg, backends::BackendFactory& factory);

    [[nodiscard]] std::vector<backends::ModelMetadata> list() const;   // FR-3
    [[nodiscard]] bool anyReady() const noexcept;                      // FR-25

    /// Non-owning lookup; nullptr if unknown (design interfaces §6).
    [[nodiscard]] backends::IInferenceBackend* find(std::string_view model_id) noexcept;

private:
    mutable std::shared_mutex mu_;
    std::map<std::string, std::unique_ptr<backends::IInferenceBackend>, std::less<>> backends_;
};

} // namespace cvis::runtime
