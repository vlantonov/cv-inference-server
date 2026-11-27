#pragma once

/**
 * @file tensor.hpp
 * @brief Tensor geometry (Shape), the owning Tensor, and the non-owning
 *        TensorView.
 * @ingroup cvis_core
 */

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cvis/core/data_type.hpp"

namespace cvis::core {

/**
 * @brief Tensor dimensions.
 *
 * A value of -1 denotes a dynamic dimension (valid only in metadata, never in
 * an allocated Tensor).
 * @ingroup cvis_core
 */
using Shape = std::vector<std::int64_t>;

/**
 * @brief Product of the (non-negative) dimensions in a shape.
 * @param shape Tensor shape.
 * @return 1 for a scalar (empty shape); 0 if any dimension is negative
 *         (an un-allocatable shape).
 * @ingroup cvis_core
 */
std::int64_t elementCount(const Shape& shape) noexcept;

/**
 * @brief Owning, move-only tensor.
 *
 * RAII over a single contiguous little-endian byte buffer sized to
 * `elementCount(shape) * byteSize(dtype)` (design interfaces §1). Copies are
 * never implicit; use clone() for an explicit deep copy.
 * @ingroup cvis_core
 */
class Tensor {
public:
    Tensor() = default;

    /**
     * @brief Allocate a zero-initialised buffer for the given geometry.
     * @param name  Logical tensor name (input/output binding).
     * @param dtype Element data type.
     * @param shape Tensor dimensions (must be non-negative to allocate).
     */
    Tensor(std::string name, DataType dtype, Shape shape);

    Tensor(Tensor&&) noexcept = default;
    Tensor& operator=(Tensor&&) noexcept = default;
    Tensor(const Tensor&) = delete;
    Tensor& operator=(const Tensor&) = delete;

    /// @brief Explicit deep copy (copies are never implicit for a Tensor).
    [[nodiscard]] Tensor clone() const;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] DataType dtype() const noexcept { return dtype_; }
    [[nodiscard]] const Shape& shape() const noexcept { return shape_; }
    [[nodiscard]] std::size_t byteCount() const noexcept { return data_.size(); }

    /// @brief Mutable view over the owned byte buffer.
    [[nodiscard]] std::span<std::byte> bytes() noexcept { return {data_.data(), data_.size()}; }
    /// @brief Read-only view over the owned byte buffer.
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
        return {data_.data(), data_.size()};
    }

private:
    std::string name_;
    DataType dtype_ = DataType::kUnspecified;
    Shape shape_;
    std::vector<std::byte> data_;
};

/**
 * @brief Non-owning, read-only view over an external buffer (e.g. a gRPC arena).
 *
 * Valid only while the source outlives it (design interfaces §1).
 * @ingroup cvis_core
 */
struct TensorView {
    std::string_view name;
    DataType dtype = DataType::kUnspecified;
    std::span<const std::int64_t> shape;
    std::span<const std::byte> data;
};

} // namespace cvis::core
