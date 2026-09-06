#include "cvis/backends/backend.hpp"

#include "cvis/backends/fake_backend.hpp"

#ifdef CVIS_ENABLE_ONNXRUNTIME
#include "cvis/backends/onnxruntime_backend.hpp"
#endif
#ifdef CVIS_ENABLE_TENSORRT
#include "cvis/backends/tensorrt_backend.hpp"
#endif

namespace cvis::backends {

void BackendFactory::registerBackend(std::string name, Creator creator) {
    creators_.insert_or_assign(std::move(name), std::move(creator));
}

bool BackendFactory::has(std::string_view name) const {
    return creators_.find(name) != creators_.end();
}

core::Expected<std::unique_ptr<IInferenceBackend>>
BackendFactory::create(std::string_view name) const {
    auto it = creators_.find(name);
    if (it == creators_.end()) {
        return core::Status::Error(
            core::StatusCode::kUnimplemented,
            "backend not built or registered: " + std::string(name));
    }
    return it->second();
}

void BackendFactory::registerBuiltins() {
    registerBackend("fake", [] { return std::make_unique<FakeBackend>(); });
#ifdef CVIS_ENABLE_ONNXRUNTIME
    registerBackend("onnxruntime", [] { return std::make_unique<OnnxRuntimeBackend>(); });
#endif
#ifdef CVIS_ENABLE_TENSORRT
    registerBackend("tensorrt", [] { return std::make_unique<TensorRtBackend>(); });
#endif
}

} // namespace cvis::backends
