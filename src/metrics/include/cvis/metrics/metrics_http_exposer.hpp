#pragma once

/**
 * @file metrics_http_exposer.hpp
 * @brief Gated HTTP server that exposes the Prometheus `/metrics` endpoint.
 * @ingroup cvis_metrics
 */

#include <cstdint>
#include <memory>
#include <string>

#include "cvis/metrics/metrics_registry.hpp"

namespace cvis::metrics {

/**
 * @brief Serves the Prometheus text exposition at GET /metrics (design §4, OQ-4).
 *
 * @note Only functional when built with `CVIS_ENABLE_METRICS_HTTP`
 *       (prometheus-cpp available). Without it, construction is a no-op stub so
 *       the composition root still builds and runs on a bare CPU machine
 *       (NFR-8).
 * @ingroup cvis_metrics
 */
class MetricsHttpExposer {
public:
    /**
     * @brief Construct and (when compiled in) start the HTTP listener.
     * @param registry  Metrics façade scraped on each request.
     * @param bind_addr Listen address.
     * @param port      Listen port.
     */
    MetricsHttpExposer(MetricsRegistry& registry, std::string bind_addr, std::uint16_t port);
    ~MetricsHttpExposer();

    MetricsHttpExposer(const MetricsHttpExposer&) = delete;
    MetricsHttpExposer& operator=(const MetricsHttpExposer&) = delete;

    /// @brief True when an HTTP listener is actually serving (compiled-in path only).
    [[nodiscard]] bool serving() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cvis::metrics
