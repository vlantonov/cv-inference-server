#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "cvis/metrics/metrics_registry.hpp"

using namespace cvis;
using namespace std::chrono_literals;

TEST(MetricsRegistry, RequestAndErrorCountersIncrement) {
    metrics::MetricsRegistry reg;
    EXPECT_EQ(reg.requests("m"), 0u);

    reg.incRequests("m");
    reg.incRequests("m");
    reg.incRequests("other");
    EXPECT_EQ(reg.requests("m"), 2u);
    EXPECT_EQ(reg.requests("other"), 1u);

    reg.incErrors("m");
    EXPECT_EQ(reg.errors("m"), 1u);
    EXPECT_EQ(reg.errors("other"), 0u);
}

TEST(MetricsRegistry, UnknownModelReadsZero) {
    metrics::MetricsRegistry reg;
    EXPECT_EQ(reg.requests("nope"), 0u);
    EXPECT_EQ(reg.errors("nope"), 0u);
    EXPECT_EQ(reg.batches("nope"), 0u);
    EXPECT_EQ(reg.queueDepth("nope"), 0u);
}

TEST(MetricsRegistry, BatchSizeObservationsAggregate) {
    metrics::MetricsRegistry reg;
    reg.observeBatchSize("m", 4);
    reg.observeBatchSize("m", 8);
    EXPECT_EQ(reg.batches("m"), 2u);

    const std::string text = reg.scrape();
    EXPECT_NE(text.find("cvis_batch_size_count{model=\"m\"} 2"), std::string::npos);
    EXPECT_NE(text.find("cvis_batch_size_sum{model=\"m\"} 12"), std::string::npos);
}

TEST(MetricsRegistry, QueueDepthGaugeIsSetNotAccumulated) {
    metrics::MetricsRegistry reg;
    reg.setQueueDepth("m", 5);
    EXPECT_EQ(reg.queueDepth("m"), 5u);
    reg.setQueueDepth("m", 2);
    EXPECT_EQ(reg.queueDepth("m"), 2u);
}

TEST(MetricsRegistry, LatencyHistogramObservesIntoBuckets) {
    metrics::MetricsRegistry reg;
    reg.observeLatency("m", 500us);   // 0.5 ms  -> le="1"
    reg.observeLatency("m", 3ms);     // 3 ms    -> le="5"
    reg.observeLatency("m", 500ms);   // 500 ms  -> +Inf

    const std::string text = reg.scrape();
    EXPECT_NE(text.find("cvis_latency_ms_count{model=\"m\"} 3"), std::string::npos);
    // Cumulative bucket le="1" holds exactly the 0.5 ms observation.
    EXPECT_NE(text.find("cvis_latency_ms_bucket{model=\"m\",le=\"1\"} 1"),
              std::string::npos);
    // le="5" is cumulative and covers the 0.5 ms and 3 ms observations.
    EXPECT_NE(text.find("cvis_latency_ms_bucket{model=\"m\",le=\"5\"} 2"),
              std::string::npos);
    // The +Inf bucket covers every observation.
    EXPECT_NE(text.find("cvis_latency_ms_bucket{model=\"m\",le=\"+Inf\"} 3"),
              std::string::npos);
}

TEST(MetricsRegistry, ScrapeEmitsPrometheusTypeHeaders) {
    metrics::MetricsRegistry reg;
    reg.incRequests("m");
    const std::string text = reg.scrape();
    EXPECT_NE(text.find("# TYPE cvis_requests_total counter"), std::string::npos);
    EXPECT_NE(text.find("# TYPE cvis_latency_ms histogram"), std::string::npos);
    EXPECT_NE(text.find("cvis_requests_total{model=\"m\"} 1"), std::string::npos);
}
