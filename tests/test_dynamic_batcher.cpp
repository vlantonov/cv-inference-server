#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <future>
#include <string>
#include <utility>
#include <vector>

#include "cvis/backends/fake_backend.hpp"
#include "cvis/batching/dynamic_batcher.hpp"
#include "cvis/core/core.hpp"
#include "cvis/metrics/metrics_registry.hpp"
#include "cvis/scheduler/scheduler.hpp"

using namespace cvis;
using namespace std::chrono_literals;

namespace {

core::InferRequest makeRequest(std::string corr, float value) {
    core::InferRequest req;
    req.model_id = "m";
    req.correlation_id = std::move(corr);
    core::Tensor t("in", core::DataType::kFloat32, core::Shape{1});
    std::memcpy(t.bytes().data(), &value, sizeof(value));
    req.inputs.push_back(std::move(t));
    return req;
}

float firstValue(const core::InferResponse& resp) {
    float v = 0.0f;
    std::memcpy(&v, resp.outputs.at(0).bytes().data(), sizeof(v));
    return v;
}

// Fixture wiring a real Scheduler + FakeBackend behind the batcher so submitted
// futures are actually fulfilled end-to-end.
struct BatcherFixture {
    metrics::MetricsRegistry metrics;
    backends::FakeBackend backend;
    scheduler::Scheduler scheduler{scheduler::SchedulerConfig{/*workers=*/1, /*max_in_flight=*/64},
                                   metrics};

    BatcherFixture() {
        backends::ModelConfig cfg;
        cfg.model_id = "m";
        backend.load(cfg);
        scheduler.registerModel("m", backend);
    }
};

} // namespace

TEST(DynamicBatcher, FlushesWhenMaxBatchSizeReached) {
    BatcherFixture fx;
    batching::BatchingConfig cfg;
    cfg.enabled = true;
    cfg.max_batch_size = 4;
    cfg.max_wait = 10s;               // deadline effectively disabled for this test
    cfg.max_queue_depth = 256;
    batching::DynamicBatcher batcher(cfg, fx.scheduler, fx.metrics);

    std::vector<std::future<core::InferResponse>> futs;
    for (int i = 0; i < 4; ++i) {
        auto submitted = batcher.submit(makeRequest("c" + std::to_string(i),
                                                    static_cast<float>(i)));
        ASSERT_TRUE(submitted.has_value());
        futs.push_back(std::move(submitted).value());
    }

    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(futs[i].wait_for(2s), std::future_status::ready);
        const core::InferResponse resp = futs[i].get();
        EXPECT_EQ(resp.correlation_id, "c" + std::to_string(i));
        EXPECT_FLOAT_EQ(firstValue(resp), static_cast<float>(i));
    }
    // The four requests coalesced into a single backend invocation.
    EXPECT_EQ(fx.backend.batchCallCount(), 1u);
}

TEST(DynamicBatcher, FlushesOnDeadlineBelowMaxBatchSize) {
    BatcherFixture fx;
    batching::BatchingConfig cfg;
    cfg.enabled = true;
    cfg.max_batch_size = 8;           // never reached
    cfg.max_wait = 20ms;              // deadline is the trigger
    cfg.max_queue_depth = 256;
    batching::DynamicBatcher batcher(cfg, fx.scheduler, fx.metrics);

    std::vector<std::future<core::InferResponse>> futs;
    for (int i = 0; i < 3; ++i) {
        auto submitted = batcher.submit(makeRequest("d" + std::to_string(i),
                                                    static_cast<float>(i)));
        ASSERT_TRUE(submitted.has_value());
        futs.push_back(std::move(submitted).value());
    }

    for (int i = 0; i < 3; ++i) {
        ASSERT_EQ(futs[i].wait_for(2s), std::future_status::ready);
        EXPECT_EQ(futs[i].get().correlation_id, "d" + std::to_string(i));
    }
    EXPECT_EQ(fx.backend.batchCallCount(), 1u);
}

TEST(DynamicBatcher, DemuxReturnsEachResponseToItsCorrelationId) {
    BatcherFixture fx;
    batching::BatchingConfig cfg;
    cfg.enabled = true;
    cfg.max_batch_size = 3;
    cfg.max_wait = 10s;
    batching::DynamicBatcher batcher(cfg, fx.scheduler, fx.metrics);

    auto f0 = batcher.submit(makeRequest("x", 10.0f));
    auto f1 = batcher.submit(makeRequest("y", 20.0f));
    auto f2 = batcher.submit(makeRequest("z", 30.0f));
    ASSERT_TRUE(f0.has_value() && f1.has_value() && f2.has_value());

    const core::InferResponse r0 = std::move(f0).value().get();
    const core::InferResponse r1 = std::move(f1).value().get();
    const core::InferResponse r2 = std::move(f2).value().get();

    EXPECT_EQ(r0.correlation_id, "x");
    EXPECT_FLOAT_EQ(firstValue(r0), 10.0f);
    EXPECT_EQ(r1.correlation_id, "y");
    EXPECT_FLOAT_EQ(firstValue(r1), 20.0f);
    EXPECT_EQ(r2.correlation_id, "z");
    EXPECT_FLOAT_EQ(firstValue(r2), 30.0f);
}

TEST(DynamicBatcher, DisabledPathRunsBatchesOfOne) {
    BatcherFixture fx;
    batching::BatchingConfig cfg;
    cfg.enabled = false;              // disable path == batch size 1 (FR-19)
    cfg.max_batch_size = 8;
    cfg.max_wait = 10s;
    batching::DynamicBatcher batcher(cfg, fx.scheduler, fx.metrics);

    std::vector<std::future<core::InferResponse>> futs;
    for (int i = 0; i < 3; ++i) {
        auto submitted = batcher.submit(makeRequest("e" + std::to_string(i),
                                                    static_cast<float>(i)));
        ASSERT_TRUE(submitted.has_value());
        futs.push_back(std::move(submitted).value());
    }

    for (int i = 0; i < 3; ++i) {
        ASSERT_EQ(futs[i].wait_for(2s), std::future_status::ready);
        const core::InferResponse resp = futs[i].get();
        EXPECT_EQ(resp.correlation_id, "e" + std::to_string(i));
        EXPECT_FLOAT_EQ(firstValue(resp), static_cast<float>(i));
    }
    // Each request dispatched on its own -> three separate backend invocations.
    EXPECT_EQ(fx.backend.batchCallCount(), 3u);
}

TEST(DynamicBatcher, BackpressureReturnsResourceExhaustedWhenQueueFull) {
    BatcherFixture fx;
    batching::BatchingConfig cfg;
    cfg.enabled = true;
    cfg.max_batch_size = 1000;        // size trigger never fires
    cfg.max_wait = 10s;               // deadline never fires during the test
    cfg.max_queue_depth = 4;          // queue saturates at 4
    batching::DynamicBatcher batcher(cfg, fx.scheduler, fx.metrics);

    std::vector<std::future<core::InferResponse>> futs;
    for (int i = 0; i < 4; ++i) {
        auto submitted = batcher.submit(makeRequest("q" + std::to_string(i),
                                                    static_cast<float>(i)));
        ASSERT_TRUE(submitted.has_value());
        futs.push_back(std::move(submitted).value());
    }

    auto overflow = batcher.submit(makeRequest("overflow", 99.0f));
    ASSERT_FALSE(overflow.has_value());
    EXPECT_EQ(overflow.error().code, core::StatusCode::kResourceExhausted);
}
