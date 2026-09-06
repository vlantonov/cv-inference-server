#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "cvis/core/core.hpp"
#include "cvis/metrics/metrics_registry.hpp"
#include "cvis/scheduler/scheduler.hpp"
#include "mock_backend.hpp"

using namespace cvis;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using namespace std::chrono_literals;

namespace {

// Echoing backend action: one response per request, correlation-id preserved.
core::Expected<std::vector<core::InferResponse>>
echoBatch(std::span<const core::InferRequest* const> batch) {
    std::vector<core::InferResponse> out;
    out.reserve(batch.size());
    for (const core::InferRequest* r : batch) {
        core::InferResponse resp;
        resp.model_id = r->model_id;
        resp.correlation_id = r->correlation_id;
        for (const auto& in : r->inputs) resp.outputs.push_back(in.clone());
        resp.status = core::Status::Ok();
        out.push_back(std::move(resp));
    }
    return out;
}

core::InferRequest makeRequest(std::string model_id, std::string corr) {
    core::InferRequest req;
    req.model_id = std::move(model_id);
    req.correlation_id = std::move(corr);
    req.inputs.emplace_back("in", core::DataType::kFloat32, core::Shape{1});
    return req;
}

// Build a BatchJob for `model_id` with the given correlation ids and return the
// index-aligned futures the scheduler will fulfil.
std::pair<scheduler::BatchJob, std::vector<std::future<core::InferResponse>>>
makeJob(const std::string& model_id, const std::vector<std::string>& corrs) {
    scheduler::BatchJob job;
    job.model_id = model_id;
    std::vector<std::future<core::InferResponse>> futs;
    for (const auto& c : corrs) {
        job.requests.push_back(makeRequest(model_id, c));
        job.promises.emplace_back();
        futs.push_back(job.promises.back().get_future());
    }
    return {std::move(job), std::move(futs)};
}

} // namespace

TEST(Scheduler, DispatchesBatchAndDemuxesByCorrelation) {
    metrics::MetricsRegistry metrics;
    NiceMock<cvis::testing::MockBackend> backend;
    EXPECT_CALL(backend, executeBatch(_)).WillRepeatedly(Invoke(echoBatch));

    scheduler::Scheduler sched(scheduler::SchedulerConfig{/*workers=*/1, /*max_in_flight=*/4},
                               metrics);
    sched.registerModel("m", backend);

    auto [job, futs] = makeJob("m", {"a", "b", "c"});
    const core::Status s = sched.enqueue(std::move(job));
    ASSERT_TRUE(s.ok());

    for (std::size_t i = 0; i < futs.size(); ++i) {
        ASSERT_EQ(futs[i].wait_for(2s), std::future_status::ready);
        const core::InferResponse resp = futs[i].get();
        EXPECT_TRUE(resp.status.ok());
        EXPECT_EQ(resp.model_id, "m");
        EXPECT_EQ(resp.outputs.size(), 1u);
    }
    EXPECT_EQ(futs[0].valid(), false); // consumed above
}

TEST(Scheduler, UnregisteredModelReturnsNotFound) {
    metrics::MetricsRegistry metrics;
    scheduler::Scheduler sched({}, metrics);

    auto [job, futs] = makeJob("ghost", {"a"});
    const core::Status s = sched.enqueue(std::move(job));
    EXPECT_EQ(s.code, core::StatusCode::kNotFound);

    // Promises are still fulfilled with the error so no future hangs.
    ASSERT_EQ(futs[0].wait_for(2s), std::future_status::ready);
    EXPECT_EQ(futs[0].get().status.code, core::StatusCode::kNotFound);
}

TEST(Scheduler, BoundedInFlightReturnsResourceExhausted) {
    metrics::MetricsRegistry metrics;
    NiceMock<cvis::testing::MockBackend> backend;

    // Gate blocks the single worker inside executeBatch so in-flight work stays
    // outstanding while we probe the backpressure boundary.
    std::promise<void> gate;
    std::shared_future<void> gate_ready = gate.get_future().share();
    EXPECT_CALL(backend, executeBatch(_))
        .WillRepeatedly(Invoke(
            [&gate_ready](std::span<const core::InferRequest* const> batch) {
                gate_ready.wait();
                return echoBatch(batch);
            }));

    scheduler::Scheduler sched(scheduler::SchedulerConfig{/*workers=*/1, /*max_in_flight=*/2},
                               metrics);
    sched.registerModel("m", backend);

    auto [job1, futs1] = makeJob("m", {"a"});
    auto [job2, futs2] = makeJob("m", {"b"});
    auto [job3, futs3] = makeJob("m", {"c"});

    EXPECT_TRUE(sched.enqueue(std::move(job1)).ok());
    EXPECT_TRUE(sched.enqueue(std::move(job2)).ok());
    const core::Status s3 = sched.enqueue(std::move(job3));
    EXPECT_EQ(s3.code, core::StatusCode::kResourceExhausted);

    // The rejected job's future carries the same status (no hang).
    ASSERT_EQ(futs3[0].wait_for(2s), std::future_status::ready);
    EXPECT_EQ(futs3[0].get().status.code, core::StatusCode::kResourceExhausted);

    // Release the worker; the two accepted jobs complete.
    gate.set_value();
    ASSERT_EQ(futs1[0].wait_for(2s), std::future_status::ready);
    ASSERT_EQ(futs2[0].wait_for(2s), std::future_status::ready);
    EXPECT_TRUE(futs1[0].get().status.ok());
    EXPECT_TRUE(futs2[0].get().status.ok());
}

TEST(Scheduler, BackendFailurePropagatesErrorAndRecordsMetrics) {
    metrics::MetricsRegistry metrics;
    NiceMock<cvis::testing::MockBackend> backend;
    // Backend rejects the whole batch (FR-2/FR-13): every promise must receive
    // the error and the failure must be counted (FR-26), with no future hanging.
    EXPECT_CALL(backend, executeBatch(_))
        .WillRepeatedly(Invoke([](std::span<const core::InferRequest* const>) {
            return core::Expected<std::vector<core::InferResponse>>(
                core::Status::Error(core::StatusCode::kInternal, "boom"));
        }));

    scheduler::Scheduler sched(scheduler::SchedulerConfig{/*workers=*/1, /*max_in_flight=*/4},
                               metrics);
    sched.registerModel("m", backend);

    auto [job, futs] = makeJob("m", {"a", "b"});
    ASSERT_TRUE(sched.enqueue(std::move(job)).ok());

    for (auto& f : futs) {
        ASSERT_EQ(f.wait_for(2s), std::future_status::ready);
        EXPECT_EQ(f.get().status.code, core::StatusCode::kInternal);
    }
    EXPECT_EQ(metrics.errors("m"), 2u);   // one per failed request
}

TEST(Scheduler, NoStarvationAcrossModels) {
    metrics::MetricsRegistry metrics;
    NiceMock<cvis::testing::MockBackend> backend_a;
    NiceMock<cvis::testing::MockBackend> backend_b;
    EXPECT_CALL(backend_a, executeBatch(_)).WillRepeatedly(Invoke(echoBatch));
    EXPECT_CALL(backend_b, executeBatch(_)).WillRepeatedly(Invoke(echoBatch));

    scheduler::Scheduler sched(scheduler::SchedulerConfig{/*workers=*/1, /*max_in_flight=*/8},
                               metrics);
    sched.registerModel("A", backend_a);
    sched.registerModel("B", backend_b);

    // Skew arrival heavily toward A; round-robin must still service B.
    std::vector<std::future<core::InferResponse>> a_futs;
    for (int i = 0; i < 6; ++i) {
        auto [job, futs] = makeJob("A", {"a" + std::to_string(i)});
        ASSERT_TRUE(sched.enqueue(std::move(job)).ok());
        a_futs.push_back(std::move(futs[0]));
    }
    auto [job_b, b_futs] = makeJob("B", {"b0"});
    ASSERT_TRUE(sched.enqueue(std::move(job_b)).ok());

    ASSERT_EQ(b_futs[0].wait_for(2s), std::future_status::ready);
    EXPECT_EQ(b_futs[0].get().correlation_id, "b0");
    for (auto& f : a_futs) {
        ASSERT_EQ(f.wait_for(2s), std::future_status::ready);
        EXPECT_TRUE(f.get().status.ok());
    }
}
