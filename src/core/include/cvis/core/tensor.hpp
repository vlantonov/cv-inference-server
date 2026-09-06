#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cvis/core/data_type.hpp"

namespace cvis::core {

/// Tensor dimensions. A value of -1 denotes a dynamic dimension (only valid in
/// metadata, never in an allocated Tensor).
using Shape = std::vector<std::int64_t>;

/// Product of the (non-negative) dimensions in \p shape. Returns 1 for a scalar
/// (empty shape). Any negative dimension yields 0 (an un-allocatable shape).
std::int64_t elementCount(const Shape& shape) noexcept;

/// Owning, move-only tensor. RAII over a single contiguous little-endian byte
/// buffer sized to elementCount(shape) * byteSize(dtype) (design interfaces §1).
class Tensor {
public:
    Tensor() = default;

    /// Allocates a zero-initialised buffer for the given geometry.
    Tensor(std::string name, DataType dtype, Shape shape);

    Tensor(Tensor&&) noexcept = default;
    Tensor& operator=(Tensor&&) noexcept = default;
    Tensor(const Tensor&) = delete;
    Tensor& operator=(const Tensor&) = delete;

    /// Explicit deep copy (copies are never implicit for a Tensor).
    [[nodiscard]] Tensor clone() const;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] DataType dtype() const noexcept { return dtype_; }
    [[nodiscard]] const Shape& shape() const noexcept { return shape_; }
    [[nodiscard]] std::size_t byteCount() const noexcept { return data_.size(); }

    [[nodiscard]] std::span<std::byte> bytes() noexcept { return {data_.data(), data_.size()}; }
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
        return {data_.data(), data_.size()};
    }

private:
    std::string name_;
    DataType dtype_ = DataType::kUnspecified;
    Shape shape_;
    std::vector<std::byte> data_;
};

/// Non-owning, read-only view over an external buffer (e.g. a gRPC arena).
/// Valid only while the source outlives it (design interfaces §1).
struct TensorView {
    std::string_view name;
    DataType dtype = DataType::kUnspecified;
    std::span<const std::int64_t> shape;
    std::span<const std::byte> data;
};

} // namespace cvis::core
