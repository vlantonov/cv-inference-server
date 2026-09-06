#include "cvis/batching/dynamic_batcher.hpp"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "cvis/metrics/metrics_registry.hpp"
#include "cvis/scheduler/scheduler.hpp"

namespace cvis::batching {
namespace {

using Clock = std::chrono::steady_clock;

struct PendingItem {
    core::InferRequest request;
    std::promise<core::InferResponse> promise;
};

struct ModelQueue {
    std::deque<PendingItem> items;
    Clock::time_point deadline{};   ///< valid only while items is non-empty
};

} // namespace

class DynamicBatcher::Impl {
public:
    Impl(BatchingConfig cfg, scheduler::Scheduler& scheduler,
         metrics::MetricsRegistry& metrics)
        : cfg_(cfg), scheduler_(scheduler), metrics_(metrics) {
        // Disable path: effective batch size of 1 (FR-19) reuses the same code.
        effective_max_batch_ = cfg_.enabled ? std::max<std::size_t>(cfg_.max_batch_size, 1)
                                             : 1;
        flush_thread_ = std::thread([this] { flushLoop(); });
    }

    ~Impl() { shutdown(); }

    core::Expected<std::future<core::InferResponse>> submit(core::InferRequest request) {
        const std::string model_id = request.model_id;
        std::unique_lock lk(mu_);
        ModelQueue& q = queues_[model_id];
        if (q.items.size() >= cfg_.max_queue_depth) {
            return core::Status::Error(core::StatusCode::kResourceExhausted,
                                       "batch queue full for model " + model_id);
        }
        if (q.items.empty()) {
            q.deadline = Clock::now() + cfg_.max_wait;
        }
        metrics_.incRequests(model_id);

        PendingItem item;
        item.request = std::move(request);
        std::future<core::InferResponse> fut = item.promise.get_future();
        q.items.push_back(std::move(item));
        metrics_.setQueueDepth(model_id, q.items.size());

        lk.unlock();
        cv_.notify_all();   // wake flush loop (size trigger or new deadline)
        return fut;
    }

    void shutdown() {
        {
            std::lock_guard lk(mu_);
            if (stopping_) return;
            stopping_ = true;
        }
        cv_.notify_all();
        if (flush_thread_.joinable()) flush_thread_.join();
        // Drain anything still queued: fail fast with UNAVAILABLE so no future hangs.
        std::lock_guard lk(mu_);
        for (auto& [id, q] : queues_) {
            while (!q.items.empty()) {
                failItem(id, q.items.front(),
                         core::Status::Error(core::StatusCode::kUnavailable,
                                             "server shutting down"));
                q.items.pop_front();
            }
        }
    }

private:
    void failItem(const std::string& model_id, PendingItem& item, core::Status status) {
        core::InferResponse resp;
        resp.model_id = model_id;
        resp.correlation_id = item.request.correlation_id;
        resp.status = std::move(status);
        item.promise.set_value(std::move(resp));
    }

    // Returns the earliest deadline among non-empty queues, or nullopt if none.
    std::optional<Clock::time_point> earliestDeadline() const {
        std::optional<Clock::time_point> earliest;
        for (const auto& [id, q] : queues_) {
            if (q.items.empty()) continue;
            if (!earliest || q.deadline < *earliest) earliest = q.deadline;
        }
        return earliest;
    }

    // Pop the next ready model (size- or deadline-triggered). Returns false if
    // nothing is ready yet.
    bool takeReadyBatch(std::string& model_id, std::vector<PendingItem>& out) {
        const auto now = Clock::now();
        for (auto& [id, q] : queues_) {
            if (q.items.empty()) continue;
            const bool ready = q.items.size() >= effective_max_batch_ || q.deadline <= now;
            if (!ready) continue;
            const std::size_t take = std::min(q.items.size(), effective_max_batch_);
            out.clear();
            out.reserve(take);
            for (std::size_t i = 0; i < take; ++i) {
                out.push_back(std::move(q.items.front()));
                q.items.pop_front();
            }
            metrics_.setQueueDepth(id, q.items.size());
            model_id = id;
            return true;
        }
        return false;
    }

    void flushLoop() {
        std::unique_lock lk(mu_);
        while (!stopping_) {
            std::string model_id;
            std::vector<PendingItem> batch;
            if (takeReadyBatch(model_id, batch)) {
                // Build the BatchJob and hand ownership to the scheduler. From
                // here the scheduler owns fulfilling every promise -- including
                // on synchronous rejection (backpressure/NOT_FOUND) -- so no
                // future is ever left unresolved (FR-23).
                scheduler::BatchJob job;
                job.model_id = model_id;
                job.requests.reserve(batch.size());
                job.promises.reserve(batch.size());
                for (auto& item : batch) {
                    job.requests.push_back(std::move(item.request));
                    job.promises.push_back(std::move(item.promise));
                }
                lk.unlock();
                (void)scheduler_.enqueue(std::move(job));
                lk.lock();
                continue;
            }
            // Nothing ready now: sleep until the earliest deadline, a new submit,
            // or shutdown. The timed wait is what makes the deadline trigger fire
            // even when no further requests arrive (FR-16/NFR-3).
            if (auto dl = earliestDeadline()) {
                cv_.wait_until(lk, *dl);
            } else {
                cv_.wait(lk);
            }
        }
    }

    BatchingConfig cfg_;
    std::size_t effective_max_batch_ = 1;
    scheduler::Scheduler& scheduler_;
    metrics::MetricsRegistry& metrics_;

    std::mutex mu_;
    std::condition_variable cv_;
    std::map<std::string, ModelQueue, std::less<>> queues_;
    bool stopping_ = false;
    std::thread flush_thread_;
};

DynamicBatcher::DynamicBatcher(BatchingConfig cfg, scheduler::Scheduler& scheduler,
                               metrics::MetricsRegistry& metrics)
    : impl_(std::make_unique<Impl>(cfg, scheduler, metrics)) {}

DynamicBatcher::~DynamicBatcher() = default;

core::Expected<std::future<core::InferResponse>>
DynamicBatcher::submit(core::InferRequest request) {
    return impl_->submit(std::move(request));
}

void DynamicBatcher::shutdown() { impl_->shutdown(); }

} // namespace cvis::batching
