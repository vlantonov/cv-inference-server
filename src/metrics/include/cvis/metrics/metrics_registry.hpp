#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

namespace cvis::metrics {

/// Thread-safe, dependency-free metrics façade (design interfaces §5).
///
/// The design proposes prometheus-cpp as the backing store; to keep the
/// scheduler/batching layers testable on a bare CPU machine this façade is
/// implemented self-contained and exposes Prometheus text via scrape(). When
/// prometheus-cpp is available the HTTP exposer (guarded, CVIS_ENABLE_METRICS_HTTP)
/// scrapes this same façade.
class MetricsRegistry {
public:
    MetricsRegistry() = default;

    MetricsRegistry(const MetricsRegistry&) = delete;
    MetricsRegistry& operator=(const MetricsRegistry&) = delete;

    void incRequests(std::string_view model_id) noexcept;                 // FR-26
    void incErrors(std::string_view model_id) noexcept;
    void observeLatency(std::string_view model_id,
                        std::chrono::nanoseconds d) noexcept;
    void observeBatchSize(std::string_view model_id, std::size_t n) noexcept;
    void setQueueDepth(std::string_view model_id, std::size_t n) noexcept;

    // --- read accessors (primarily for tests / bench) ---
    [[nodiscard]] std::uint64_t requests(std::string_view model_id) const;
    [[nodiscard]] std::uint64_t errors(std::string_view model_id) const;
    [[nodiscard]] std::uint64_t batches(std::string_view model_id) const;
    [[nodiscard]] std::size_t queueDepth(std::string_view model_id) const;

    /// Renders Prometheus text exposition format (design OQ-4).
    [[nodiscard]] std::string scrape() const;

private:
    // Fixed histogram buckets in milliseconds for latency (design §3, FR-27).
    static constexpr std::array<double, 8> kLatencyBucketsMs{
        1.0, 2.0, 5.0, 10.0, 25.0, 50.0, 100.0, 250.0};

    struct ModelMetrics {
        std::uint64_t requests = 0;
        std::uint64_t errors = 0;
        std::uint64_t batch_count = 0;
        std::uint64_t batch_size_sum = 0;
        std::uint64_t latency_count = 0;
        double latency_sum_ms = 0.0;
        std::array<std::uint64_t, kLatencyBucketsMs.size() + 1> latency_buckets{};
        std::size_t queue_depth = 0;
    };

    ModelMetrics& forModel(std::string_view model_id);

    mutable std::mutex mu_;
    std::map<std::string, ModelMetrics, std::less<>> models_;
};

} // namespace cvis::metrics
