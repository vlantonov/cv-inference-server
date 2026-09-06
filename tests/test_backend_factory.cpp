#include <gtest/gtest.h>

#include <string>

#include "cvis/backends/backend.hpp"
#include "cvis/core/status.hpp"

using namespace cvis;

// FR-9: a requested backend that was not built/registered must fail with a clear
// reason (never crash). Independent coverage of the abstraction boundary (FR-8).
TEST(BackendFactory, UnknownBackendFailsGracefullyWithReason) {
    backends::BackendFactory factory;
    factory.registerBuiltins();

    auto result = factory.create("does-not-exist");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, core::StatusCode::kUnimplemented);
    EXPECT_NE(result.error().message.find("does-not-exist"), std::string::npos);
}

TEST(BackendFactory, BuiltinFakeBackendIsAvailableAndConstructs) {
    backends::BackendFactory factory;
    factory.registerBuiltins();

    EXPECT_TRUE(factory.has("fake"));
    auto result = factory.create("fake");
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value(), nullptr);
}

// FR-9/NFR-8: on a CPU-only build the GPU backends are not compiled in, so they
// must be absent from the factory and fail gracefully rather than crash.
TEST(BackendFactory, GpuBackendsUnavailableOnCpuOnlyBuild) {
    backends::BackendFactory factory;
    factory.registerBuiltins();

#ifndef CVIS_ENABLE_ONNXRUNTIME
    EXPECT_FALSE(factory.has("onnxruntime"));
    EXPECT_FALSE(factory.create("onnxruntime").has_value());
#endif
#ifndef CVIS_ENABLE_TENSORRT
    EXPECT_FALSE(factory.has("tensorrt"));
    EXPECT_FALSE(factory.create("tensorrt").has_value());
#endif
}
