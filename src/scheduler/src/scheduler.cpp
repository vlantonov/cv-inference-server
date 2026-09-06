#include "cvis/scheduler/scheduler.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <span>
#include <thread>

#include "cvis/metrics/metrics_registry.hpp"

namespace cvis::scheduler {

// Per-model dispatch state. Backend execution is serialised per model
// (in_flight is 0 or 1); max_in_flight bounds total outstanding batches for
// backpressure (design §9).
struct Model {
    backends::IInferenceBackend* backend = nullptr;
    std::deque<BatchJob> queue;
    std::size_t in_flight = 0;

    [[nodiscard]] std::size_t outstanding() const noexcept { return queue.size() + in_flight; }
    [[nodiscard]] bool dispatchable() const noexcept { return !queue.empty() && in_flight == 0; }
};

class Scheduler::Impl {
public:
    Impl(SchedulerConfig cfg, metrics::MetricsRegistry& metrics)
        : cfg_(cfg), metrics_(metrics) {
        const std::size_t n = cfg_.workers == 0 ? 1 : cfg_.workers;
        workers_.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    ~Impl() { shutdown(); }

    void registerModel(std::string model_id, backends::IInferenceBackend& backend) {
        std::lock_guard lk(mu_);
        Model& m = models_[model_id];
        m.backend = &backend;
        if (std::find(order_.begin(), order_.end(), model_id) == order_.end()) {
            order_.push_back(std::move(model_id));
        }
    }

    core::Status enqueue(BatchJob job) {
        std::unique_lock lk(mu_);
        auto it = models_.find(job.model_id);
        if (it == models_.end() || it->second.backend == nullptr) {
            lk.unlock();
            core::Status s = core::Status::Error(core::StatusCode::kNotFound,
                                                 "model not registered: " + job.model_id);
            failJob(job, s);
            return s;
        }
        Model& m = it->second;
        if (m.outstanding() >= cfg_.max_in_flight) {
            lk.unlock();
            core::Status s = core::Status::Error(core::StatusCode::kResourceExhausted,
                                                 "in-flight batch budget exhausted for " + job.model_id);
            failJob(job, s);
            return s;
        }
        m.queue.push_back(std::move(job));
        lk.unlock();
        cv_.notify_one();
        return core::Status::Ok();
    }

    void shutdown() {
        {
            std::lock_guard lk(mu_);
            if (stopping_) return;
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();
    }

private:
    // Fulfil every promise in a job with an error response carrying `status`
    // (used on synchronous rejection and on backend failure). The scheduler
    // owns fulfilling every promise once a job is handed to enqueue.
    void failJob(BatchJob& job, const core::Status& status) {
        for (std::size_t i = 0; i < job.promises.size(); ++i) {
            metrics_.incErrors(job.model_id);
            core::InferResponse err;
            err.model_id = job.model_id;
            err.correlation_id =
                i < job.requests.size() ? job.requests[i].correlation_id : std::string{};
            err.status = status;
            job.promises[i].set_value(std::move(err));
        }
    }

    // Round-robin selection of the next dispatchable model (no starvation, FR-21).
    Model* pickModel(std::string& picked_id) {
        const std::size_t count = order_.size();
        for (std::size_t step = 0; step < count; ++step) {
            const std::size_t idx = (cursor_ + step) % count;
            Model& m = models_[order_[idx]];
            if (m.dispatchable()) {
                cursor_ = (idx + 1) % count;
                picked_id = order_[idx];
                return &m;
            }
        }
        return nullptr;
    }

    void workerLoop() {
        for (;;) {
            std::unique_lock lk(mu_);
            std::string model_id;
            Model* model = nullptr;
            cv_.wait(lk, [&] {
                if (stopping_ && !anyDispatchable()) return true;
                model = pickModel(model_id);
                return model != nullptr;
            });
            if (model == nullptr) {
                // Woken for shutdown with nothing left to dispatch.
                return;
            }

            BatchJob job = std::move(model->queue.front());
            model->queue.pop_front();
            ++model->in_flight;
            backends::IInferenceBackend* backend = model->backend;
            lk.unlock();

            dispatch(model_id, backend, std::move(job));

            lk.lock();
            --model->in_flight;
            lk.unlock();
            cv_.notify_all();
        }
    }

    bool anyDispatchable() {
        for (const auto& id : order_) {
            if (models_[id].dispatchable()) return true;
        }
        return false;
    }

    void dispatch(const std::string& model_id, backends::IInferenceBackend* backend,
                  BatchJob job) {
        metrics_.observeBatchSize(model_id, job.requests.size());

        std::vector<const core::InferRequest*> ptrs;
        ptrs.reserve(job.requests.size());
        for (const auto& r : job.requests) ptrs.push_back(&r);

        const auto t0 = std::chrono::steady_clock::now();
        auto result = backend->executeBatch(
            std::span<const core::InferRequest* const>(ptrs.data(), ptrs.size()));
        const auto elapsed = std::chrono::steady_clock::now() - t0;

        if (result) {
            std::vector<core::InferResponse>& responses = *result;
            const bool aligned = responses.size() == job.promises.size();
            for (std::size_t i = 0; i < job.promises.size(); ++i) {
                metrics_.observeLatency(model_id, elapsed);
                if (aligned && responses[i].status.ok()) {
                    job.promises[i].set_value(std::move(responses[i]));
                } else {
                    metrics_.incErrors(model_id);
                    core::InferResponse err;
                    err.model_id = model_id;
                    err.correlation_id = job.requests[i].correlation_id;
                    err.status = aligned ? responses[i].status
                                         : core::Status::Error(core::StatusCode::kInternal,
                                                               "backend returned misaligned batch");
                    job.promises[i].set_value(std::move(err));
                }
            }
        } else {
            failJob(job, result.error());
        }
    }

    SchedulerConfig cfg_;
    metrics::MetricsRegistry& metrics_;

    std::mutex mu_;
    std::condition_variable cv_;
    std::map<std::string, Model, std::less<>> models_;
    std::vector<std::string> order_;
    std::size_t cursor_ = 0;
    bool stopping_ = false;
    std::vector<std::thread> workers_;
};

Scheduler::Scheduler(SchedulerConfig cfg, metrics::MetricsRegistry& metrics)
    : impl_(std::make_unique<Impl>(cfg, metrics)) {}

Scheduler::~Scheduler() = default;

void Scheduler::registerModel(std::string model_id, backends::IInferenceBackend& backend) {
    impl_->registerModel(std::move(model_id), backend);
}

core::Status Scheduler::enqueue(BatchJob job) { return impl_->enqueue(std::move(job)); }

void Scheduler::shutdown() { impl_->shutdown(); }

} // namespace cvis::scheduler
