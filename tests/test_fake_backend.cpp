#include <gtest/gtest.h>

#include <cstring>
#include <span>
#include <vector>

#include "cvis/backends/fake_backend.hpp"
#include "cvis/core/core.hpp"

using namespace cvis;

namespace {

core::InferRequest makeRequest(std::string model_id, std::string corr,
                               std::vector<float> values) {
    core::InferRequest req;
    req.model_id = std::move(model_id);
    req.correlation_id = std::move(corr);
    core::Tensor t("input", core::DataType::kFloat32,
                   core::Shape{static_cast<std::int64_t>(values.size())});
    std::memcpy(t.bytes().data(), values.data(), values.size() * sizeof(float));
    req.inputs.push_back(std::move(t));
    return req;
}

core::Expected<std::vector<core::InferResponse>>
execute(backends::FakeBackend& backend, const std::vector<core::InferRequest>& reqs) {
    std::vector<const core::InferRequest*> ptrs;
    ptrs.reserve(reqs.size());
    for (const auto& r : reqs) ptrs.push_back(&r);
    return backend.executeBatch({ptrs.data(), ptrs.size()});
}

} // namespace

TEST(FakeBackend, LoadPopulatesMetadata) {
    backends::FakeBackend backend;
    backends::ModelConfig cfg;
    cfg.model_id = "resnet";
    cfg.backend = "fake";

    const core::Status s = backend.load(cfg);
    EXPECT_TRUE(s.ok());

    const backends::ModelMetadata& meta = backend.metadata();
    EXPECT_EQ(meta.model_id, "resnet");
    EXPECT_EQ(meta.backend, "fake");
    EXPECT_TRUE(meta.ready);
}

TEST(FakeBackend, EchoesInputsPreservingCorrelation) {
    backends::FakeBackend backend;
    backends::ModelConfig cfg;
    cfg.model_id = "m";
    backend.load(cfg);

    std::vector<core::InferRequest> reqs;
    reqs.push_back(makeRequest("m", "a", {1.0f, 2.0f, 3.0f}));
    reqs.push_back(makeRequest("m", "b", {4.0f, 5.0f}));

    auto result = execute(backend, reqs);
    ASSERT_TRUE(result.has_value());
    const auto& out = result.value();
    ASSERT_EQ(out.size(), 2u);

    EXPECT_EQ(out[0].correlation_id, "a");
    EXPECT_EQ(out[0].model_id, "m");
    EXPECT_TRUE(out[0].status.ok());
    ASSERT_EQ(out[0].outputs.size(), 1u);

    float echoed[3] = {};
    std::memcpy(echoed, out[0].outputs[0].bytes().data(), sizeof(echoed));
    EXPECT_FLOAT_EQ(echoed[0], 1.0f);
    EXPECT_FLOAT_EQ(echoed[2], 3.0f);

    EXPECT_EQ(out[1].correlation_id, "b");
    ASSERT_EQ(out[1].outputs.size(), 1u);
    EXPECT_EQ(out[1].outputs[0].byteCount(), 2u * sizeof(float));
}

TEST(FakeBackend, OutputsAreIndependentDeepCopies) {
    backends::FakeBackend backend;
    backends::ModelConfig cfg;
    cfg.model_id = "m";
    backend.load(cfg);

    std::vector<core::InferRequest> reqs;
    reqs.push_back(makeRequest("m", "a", {7.0f}));

    auto result = execute(backend, reqs);
    ASSERT_TRUE(result.has_value());
    // Mutating the request input after execution must not change the output.
    float new_val = 99.0f;
    std::memcpy(reqs[0].inputs[0].bytes().data(), &new_val, sizeof(new_val));

    float echoed = 0.0f;
    std::memcpy(&echoed, result.value()[0].outputs[0].bytes().data(), sizeof(echoed));
    EXPECT_FLOAT_EQ(echoed, 7.0f);
}

TEST(FakeBackend, CountsBatchCalls) {
    backends::FakeBackend backend;
    backends::ModelConfig cfg;
    cfg.model_id = "m";
    backend.load(cfg);

    std::vector<core::InferRequest> reqs;
    reqs.push_back(makeRequest("m", "a", {1.0f}));

    EXPECT_EQ(backend.batchCallCount(), 0u);
    (void)execute(backend, reqs);
    (void)execute(backend, reqs);
    EXPECT_EQ(backend.batchCallCount(), 2u);
}

TEST(FakeBackend, NullRequestInBatchIsInternalError) {
    backends::FakeBackend backend;
    backends::ModelConfig cfg;
    cfg.model_id = "m";
    backend.load(cfg);

    std::vector<const core::InferRequest*> ptrs{nullptr};
    auto result = backend.executeBatch({ptrs.data(), ptrs.size()});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, core::StatusCode::kInternal);
}
