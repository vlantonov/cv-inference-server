#pragma once

#include <cstddef>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include "cvis/backends/backend.hpp"
#include "cvis/core/infer.hpp"
#include "cvis/core/status.hpp"

namespace cvis::metrics { class MetricsRegistry; }

namespace cvis::scheduler {

/// Tuning for the dispatch pool (design interfaces §4).
struct SchedulerConfig {
    std::size_t workers = 1;         ///< CPU-bound ORT default = 1 (design §6)
    std::size_t max_in_flight = 4;   ///< bounded outstanding batches per model (FR-23)
};

/// One coalesced unit of work handed from the batcher to the scheduler. Requests
/// and their index-aligned promises are moved in; the scheduler fulfils each
/// promise after executeBatch, demuxing outputs by index (FR-18).
struct BatchJob {
    std::string model_id;
    std::vector<core::InferRequest> requests;
    std::vector<std::promise<core::InferResponse>> promises;
};

/// Owns worker threads. Fair (round-robin) pop across per-model queues so no
/// model starves (FR-21); backend execution is serialised per model (design §9);
/// outstanding batches per model are bounded for backpressure (FR-23).
class Scheduler {
public:
    Scheduler(SchedulerConfig cfg, metrics::MetricsRegistry& metrics);
    ~Scheduler();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    /// Bind a model_id to the backend that executes its batches. Startup only.
    void registerModel(std::string model_id, backends::IInferenceBackend& backend);

    /// THREAD-SAFE. Enqueue a batch. Returns RESOURCE_EXHAUSTED when the model's
    /// outstanding batch budget (max_in_flight) is already full (FR-23); NOT_FOUND
    /// if the model was never registered.
    core::Status enqueue(BatchJob job);

    /// Drain pending work, join workers. Idempotent; also invoked by the dtor.
    void shutdown();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cvis::scheduler
