#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "cvis/metrics/metrics_registry.hpp"

namespace cvis::metrics {

/// Serves the Prometheus text exposition at GET /metrics (design §4, OQ-4).
///
/// Only functional when the project is built with CVIS_ENABLE_METRICS_HTTP
/// (prometheus-cpp available). Without it, construction is a no-op stub so the
/// composition root builds and runs on a bare CPU machine (NFR-8).
class MetricsHttpExposer {
public:
    MetricsHttpExposer(MetricsRegistry& registry, std::string bind_addr, std::uint16_t port);
    ~MetricsHttpExposer();

    MetricsHttpExposer(const MetricsHttpExposer&) = delete;
    MetricsHttpExposer& operator=(const MetricsHttpExposer&) = delete;

    /// True when an HTTP listener is actually serving (compiled-in path only).
    [[nodiscard]] bool serving() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cvis::metrics
