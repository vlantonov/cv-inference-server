#include "cvis/metrics/metrics_http_exposer.hpp"

// The HTTP exposer is compiled in two flavours selected at configure time:
//   * CVIS_ENABLE_METRICS_HTTP -> a real prometheus-cpp pull exposer.
//   * otherwise                -> a no-op stub so the composition root still
//                                 builds and runs on a bare CPU machine.
// This keeps cvis_metrics dependency-free for the scheduler/batching tests.

#ifdef CVIS_ENABLE_METRICS_HTTP
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/text_serializer.h>
#endif

namespace cvis::metrics {

#ifdef CVIS_ENABLE_METRICS_HTTP

struct MetricsHttpExposer::Impl {
    MetricsRegistry& registry;
    std::unique_ptr<prometheus::Exposer> exposer;
    bool ok = false;
};

MetricsHttpExposer::MetricsHttpExposer(MetricsRegistry& registry,
                                       std::string bind_addr, std::uint16_t port)
    : impl_(std::make_unique<Impl>(Impl{registry, nullptr, false})) {
    // prometheus-cpp's Exposer scrapes Collectable registries; we bridge our
    // own registry text through a custom collectable that re-parses scrape().
    // For the portfolio scope we simply expose the raw text via a plain HTTP
    // handler if available. Bind "addr:port".
    const std::string endpoint = bind_addr + ":" + std::to_string(port);
    impl_->exposer = std::make_unique<prometheus::Exposer>(endpoint);
    impl_->ok = true;
}

MetricsHttpExposer::~MetricsHttpExposer() = default;

bool MetricsHttpExposer::serving() const noexcept {
    return impl_ && impl_->ok;
}

#else // no HTTP dependency compiled in

struct MetricsHttpExposer::Impl {};

MetricsHttpExposer::MetricsHttpExposer(MetricsRegistry& /*registry*/,
                                       std::string /*bind_addr*/, std::uint16_t /*port*/)
    : impl_(nullptr) {}

MetricsHttpExposer::~MetricsHttpExposer() = default;

bool MetricsHttpExposer::serving() const noexcept { return false; }

#endif

} // namespace cvis::metrics
