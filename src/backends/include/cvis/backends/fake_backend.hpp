#pragma once

#include "cvis/backends/backend.hpp"

namespace cvis::backends {

/// Deterministic echo backend used to exercise the batching/scheduler pipeline
/// on CPU CI without any external dependency (design interfaces §2, FR-8/NFR-10).
///
/// executeBatch() returns, for each request, a response whose outputs are deep
/// copies of the request inputs, preserving model_id and correlation_id so the
/// demux contract (FR-18) can be verified precisely.
class FakeBackend final : public IInferenceBackend {
public:
    FakeBackend() = default;

    core::Status load(const ModelConfig& cfg) override;
    [[nodiscard]] const ModelMetadata& metadata() const noexcept override;
    core::Expected<std::vector<core::InferResponse>>
    executeBatch(std::span<const core::InferRequest* const> batch) override;

    /// Number of times executeBatch has been called (test observability).
    [[nodiscard]] std::size_t batchCallCount() const noexcept { return batch_calls_; }

private:
    ModelMetadata metadata_{};
    std::size_t batch_calls_ = 0;
};

} // namespace cvis::backends
