#pragma once

#include <gmock/gmock.h>

#include "cvis/backends/backend.hpp"

namespace cvis::testing {

/// GoogleMock backend for expectation-based batching/scheduler tests (design §5.3).
class MockBackend : public backends::IInferenceBackend {
public:
    MOCK_METHOD(core::Status, load, (const backends::ModelConfig& cfg), (override));
    MOCK_METHOD(const backends::ModelMetadata&, metadata, (), (const, noexcept, override));
    MOCK_METHOD((core::Expected<std::vector<core::InferResponse>>), executeBatch,
                (std::span<const core::InferRequest* const> batch), (override));
};

} // namespace cvis::testing
