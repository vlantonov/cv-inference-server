#include <gtest/gtest.h>

#include <cstring>

#include "cvis/core/core.hpp"

using namespace cvis::core;

TEST(DataType, ByteSizeMatchesWidth) {
    EXPECT_EQ(byteSize(DataType::kFloat32), 4u);
    EXPECT_EQ(byteSize(DataType::kFloat16), 2u);
    EXPECT_EQ(byteSize(DataType::kInt8), 1u);
    EXPECT_EQ(byteSize(DataType::kUInt8), 1u);
    EXPECT_EQ(byteSize(DataType::kInt32), 4u);
    EXPECT_EQ(byteSize(DataType::kInt64), 8u);
    EXPECT_EQ(byteSize(DataType::kUnspecified), 0u);
}

TEST(Shape, ElementCount) {
    EXPECT_EQ(elementCount(Shape{}), 1);          // scalar
    EXPECT_EQ(elementCount(Shape{2, 3, 4}), 24);
    EXPECT_EQ(elementCount(Shape{-1, 3}), 0);     // dynamic -> un-allocatable
}

TEST(Tensor, AllocatesCorrectByteCount) {
    Tensor t("input", DataType::kFloat32, Shape{2, 3});
    EXPECT_EQ(t.name(), "input");
    EXPECT_EQ(t.dtype(), DataType::kFloat32);
    EXPECT_EQ(t.shape(), (Shape{2, 3}));
    EXPECT_EQ(t.byteCount(), 2u * 3u * 4u);
}

TEST(Tensor, BytesAreZeroInitialisedAndMutable) {
    Tensor t("x", DataType::kInt32, Shape{4});
    auto bytes = t.bytes();
    ASSERT_EQ(bytes.size(), 16u);
    for (auto b : bytes) EXPECT_EQ(std::to_integer<int>(b), 0);

    std::int32_t values[4] = {1, 2, 3, 4};
    std::memcpy(bytes.data(), values, sizeof(values));
    const Tensor& ct = t;
    std::int32_t out[4] = {};
    std::memcpy(out, ct.bytes().data(), sizeof(out));
    EXPECT_EQ(out[2], 3);
}

TEST(Tensor, CloneIsDeepCopy) {
    Tensor t("x", DataType::kUInt8, Shape{3});
    auto b = t.bytes();
    b[0] = std::byte{7};
    Tensor c = t.clone();
    EXPECT_EQ(c.name(), t.name());
    EXPECT_EQ(c.shape(), t.shape());
    EXPECT_EQ(std::to_integer<int>(c.bytes()[0]), 7);
    // Mutating the clone does not affect the original (independent buffers).
    c.bytes()[0] = std::byte{9};
    EXPECT_EQ(std::to_integer<int>(t.bytes()[0]), 7);
}

TEST(Tensor, IsMoveOnlyPreservingBuffer) {
    Tensor t("x", DataType::kFloat32, Shape{2});
    t.bytes()[0] = std::byte{0xAB};
    Tensor moved = std::move(t);
    EXPECT_EQ(moved.byteCount(), 8u);
    EXPECT_EQ(std::to_integer<int>(moved.bytes()[0]), 0xAB);
    static_assert(!std::is_copy_constructible_v<Tensor>);
    static_assert(std::is_move_constructible_v<Tensor>);
}

TEST(Expected, HoldsValueOrStatus) {
    Expected<int> ok(42);
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(ok.value(), 42);

    Expected<int> err(Status::Error(StatusCode::kNotFound, "nope"));
    ASSERT_FALSE(err.has_value());
    EXPECT_EQ(err.error().code, StatusCode::kNotFound);
    EXPECT_EQ(err.error().message, "nope");
}

TEST(Status, OkHelpers) {
    EXPECT_TRUE(Status::Ok().ok());
    EXPECT_FALSE(Status::Error(StatusCode::kInternal, "x").ok());
}
