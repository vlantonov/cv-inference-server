#pragma once

/**
 * @file scheduler.hpp
 * @brief SchedulerConfig, the BatchJob unit of work, and the fair Scheduler.
 * @ingroup cvis_scheduler
 */

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

/**
 * @brief Tuning for the dispatch pool (design interfaces §4).
 * @ingroup cvis_scheduler
 */
struct SchedulerConfig {
    std::size_t workers = 1;         ///< CPU-bound ORT default = 1 (design §6)
    std::size_t max_in_flight = 4;   ///< bounded outstanding batches per model (FR-23)
};

/**
 * @brief One coalesced unit of work handed from the batcher to the scheduler.
 *
 * Requests and their index-aligned promises are moved in; the scheduler fulfils
 * each promise after executeBatch, demuxing outputs by index (FR-18).
 * @ingroup cvis_scheduler
 */
struct BatchJob {
    std::string model_id;
    std::vector<core::InferRequest> requests;
    std::vector<std::promise<core::InferResponse>> promises;
};

/**
 * @brief Owns worker threads and dispatches batches fairly across models.
 *
 * Fair (round-robin) pop across per-model queues so no model starves (FR-21);
 * backend execution is serialised per model (design §9); outstanding batches
 * per model are bounded for backpressure (FR-23).
 * @ingroup cvis_scheduler
 */
class Scheduler {
public:
    /**
     * @brief Construct and start the worker pool.
     * @param cfg     Dispatch tuning.
     * @param metrics Registry for latency/queue metrics.
     */
    Scheduler(SchedulerConfig cfg, metrics::MetricsRegistry& metrics);
    ~Scheduler();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    /**
     * @brief Bind a model_id to the backend that executes its batches.
     *        Startup only.
     * @param model_id Model identifier.
     * @param backend  Backend instance (borrowed; must outlive the scheduler).
     */
    void registerModel(std::string model_id, backends::IInferenceBackend& backend);

    /**
     * @brief Enqueue a batch for execution. THREAD-SAFE.
     * @param job Batch of requests with index-aligned promises, moved in.
     * @return RESOURCE_EXHAUSTED when the model's outstanding batch budget
     *         (max_in_flight) is already full (FR-23); NOT_FOUND if the model
     *         was never registered; OK otherwise.
     */
    core::Status enqueue(BatchJob job);

    /**
     * @brief Drain pending work and join workers.
     *
     * Idempotent; also invoked by the destructor.
     */
    void shutdown();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cvis::scheduler
