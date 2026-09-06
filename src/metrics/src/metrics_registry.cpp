#include "cvis/metrics/metrics_registry.hpp"

#include <sstream>

namespace cvis::metrics {

MetricsRegistry::ModelMetrics& MetricsRegistry::forModel(std::string_view model_id) {
    auto it = models_.find(model_id);
    if (it == models_.end()) {
        it = models_.emplace(std::string(model_id), ModelMetrics{}).first;
    }
    return it->second;
}

void MetricsRegistry::incRequests(std::string_view model_id) noexcept {
    std::lock_guard lk(mu_);
    ++forModel(model_id).requests;
}

void MetricsRegistry::incErrors(std::string_view model_id) noexcept {
    std::lock_guard lk(mu_);
    ++forModel(model_id).errors;
}

void MetricsRegistry::observeLatency(std::string_view model_id,
                                     std::chrono::nanoseconds d) noexcept {
    const double ms =
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(d)
            .count();
    std::lock_guard lk(mu_);
    ModelMetrics& m = forModel(model_id);
    ++m.latency_count;
    m.latency_sum_ms += ms;
    std::size_t bucket = kLatencyBucketsMs.size(); // +Inf bucket
    for (std::size_t i = 0; i < kLatencyBucketsMs.size(); ++i) {
        if (ms <= kLatencyBucketsMs[i]) {
            bucket = i;
            break;
        }
    }
    ++m.latency_buckets[bucket];
}

void MetricsRegistry::observeBatchSize(std::string_view model_id, std::size_t n) noexcept {
    std::lock_guard lk(mu_);
    ModelMetrics& m = forModel(model_id);
    ++m.batch_count;
    m.batch_size_sum += n;
}

void MetricsRegistry::setQueueDepth(std::string_view model_id, std::size_t n) noexcept {
    std::lock_guard lk(mu_);
    forModel(model_id).queue_depth = n;
}

std::uint64_t MetricsRegistry::requests(std::string_view model_id) const {
    std::lock_guard lk(mu_);
    auto it = models_.find(model_id);
    return it == models_.end() ? 0 : it->second.requests;
}

std::uint64_t MetricsRegistry::errors(std::string_view model_id) const {
    std::lock_guard lk(mu_);
    auto it = models_.find(model_id);
    return it == models_.end() ? 0 : it->second.errors;
}

std::uint64_t MetricsRegistry::batches(std::string_view model_id) const {
    std::lock_guard lk(mu_);
    auto it = models_.find(model_id);
    return it == models_.end() ? 0 : it->second.batch_count;
}

std::size_t MetricsRegistry::queueDepth(std::string_view model_id) const {
    std::lock_guard lk(mu_);
    auto it = models_.find(model_id);
    return it == models_.end() ? 0 : it->second.queue_depth;
}

std::string MetricsRegistry::scrape() const {
    std::lock_guard lk(mu_);
    std::ostringstream os;

    os << "# HELP cvis_requests_total Total inference requests received.\n";
    os << "# TYPE cvis_requests_total counter\n";
    for (const auto& [id, m] : models_) {
        os << "cvis_requests_total{model=\"" << id << "\"} " << m.requests << "\n";
    }

    os << "# HELP cvis_errors_total Total inference errors.\n";
    os << "# TYPE cvis_errors_total counter\n";
    for (const auto& [id, m] : models_) {
        os << "cvis_errors_total{model=\"" << id << "\"} " << m.errors << "\n";
    }

    os << "# HELP cvis_queue_depth Current pending requests in a model queue.\n";
    os << "# TYPE cvis_queue_depth gauge\n";
    for (const auto& [id, m] : models_) {
        os << "cvis_queue_depth{model=\"" << id << "\"} " << m.queue_depth << "\n";
    }

    os << "# HELP cvis_batch_size Effective batch size per dispatched batch.\n";
    os << "# TYPE cvis_batch_size summary\n";
    for (const auto& [id, m] : models_) {
        os << "cvis_batch_size_sum{model=\"" << id << "\"} " << m.batch_size_sum << "\n";
        os << "cvis_batch_size_count{model=\"" << id << "\"} " << m.batch_count << "\n";
    }

    os << "# HELP cvis_latency_ms Request latency in milliseconds.\n";
    os << "# TYPE cvis_latency_ms histogram\n";
    for (const auto& [id, m] : models_) {
        std::uint64_t cumulative = 0;
        for (std::size_t i = 0; i < kLatencyBucketsMs.size(); ++i) {
            cumulative += m.latency_buckets[i];
            os << "cvis_latency_ms_bucket{model=\"" << id << "\",le=\""
               << kLatencyBucketsMs[i] << "\"} " << cumulative << "\n";
        }
        cumulative += m.latency_buckets[kLatencyBucketsMs.size()];
        os << "cvis_latency_ms_bucket{model=\"" << id << "\",le=\"+Inf\"} "
           << cumulative << "\n";
        os << "cvis_latency_ms_sum{model=\"" << id << "\"} " << m.latency_sum_ms << "\n";
        os << "cvis_latency_ms_count{model=\"" << id << "\"} " << m.latency_count << "\n";
    }

    return os.str();
}

} // namespace cvis::metrics
