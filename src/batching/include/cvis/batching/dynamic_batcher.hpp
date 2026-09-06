#pragma once

#include <chrono>
#include <cstddef>
#include <future>
#include <memory>

#include "cvis/core/expected.hpp"
#include "cvis/core/infer.hpp"

namespace cvis::scheduler { class Scheduler; }
namespace cvis::metrics { class MetricsRegistry; }

namespace cvis::batching {

/// Batching tuning (design interfaces §3).
struct BatchingConfig {
    bool enabled = true;                        ///< false == batch size 1 (FR-19)
    std::size_t max_batch_size = 8;             ///< size trigger (FR-16)
    std::chrono::milliseconds max_wait{5};      ///< deadline / latency budget (FR-16/NFR-3)
    std::size_t max_queue_depth = 256;          ///< bounded queue -> backpressure (FR-23)
};

/// Thread-safe front door for submitting requests. Coalesces per model_id and
/// flushes to the Scheduler on size OR deadline (design §7). The disable path
/// (enabled == false, or max_batch_size == 1) uses the SAME code path so batched
/// and unbatched results are provably identical (FR-17/FR-19).
class DynamicBatcher {
public:
    DynamicBatcher(BatchingConfig cfg,
                   scheduler::Scheduler& scheduler,
                   metrics::MetricsRegistry& metrics);
    ~DynamicBatcher();

    DynamicBatcher(const DynamicBatcher&) = delete;
    DynamicBatcher& operator=(const DynamicBatcher&) = delete;

    /// THREAD-SAFE. Returns a future fulfilled when this request's slice of the
    /// batch result is ready, or RESOURCE_EXHAUSTED immediately if the model's
    /// queue is full (FR-23).
    core::Expected<std::future<core::InferResponse>>
    submit(core::InferRequest request);

    /// Drain queued requests + stop the flush thread. Idempotent; also invoked
    /// by the destructor (RAII).
    void shutdown();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cvis::batching
