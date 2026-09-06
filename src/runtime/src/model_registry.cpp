#include "cvis/runtime/model_registry.hpp"

namespace cvis::runtime {

core::Status ModelRegistry::loadFromConfig(const Config& cfg, backends::BackendFactory& factory) {
    std::unique_lock lk(mu_);
    for (const auto& mc : cfg.models) {
        auto created = factory.create(mc.backend);
        if (!created) {
            return core::Status::Error(created.error().code,
                                       "model '" + mc.model_id + "': " + created.error().message);
        }
        std::unique_ptr<backends::IInferenceBackend> backend = std::move(created).value();
        core::Status s = backend->load(mc);
        if (!s.ok()) {
            return core::Status::Error(s.code,
                                       "model '" + mc.model_id + "' load failed: " + s.message);
        }
        backends_.insert_or_assign(mc.model_id, std::move(backend));
    }
    return core::Status::Ok();
}

std::vector<backends::ModelMetadata> ModelRegistry::list() const {
    std::shared_lock lk(mu_);
    std::vector<backends::ModelMetadata> out;
    out.reserve(backends_.size());
    for (const auto& [id, backend] : backends_) {
        out.push_back(backend->metadata());
    }
    return out;
}

bool ModelRegistry::anyReady() const noexcept {
    std::shared_lock lk(mu_);
    for (const auto& [id, backend] : backends_) {
        if (backend->metadata().ready) return true;
    }
    return false;
}

backends::IInferenceBackend* ModelRegistry::find(std::string_view model_id) noexcept {
    std::shared_lock lk(mu_);
    auto it = backends_.find(model_id);
    return it == backends_.end() ? nullptr : it->second.get();
}

} // namespace cvis::runtime
