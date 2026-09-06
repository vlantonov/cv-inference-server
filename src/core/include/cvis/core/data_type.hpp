#pragma once

#include <cstddef>

namespace cvis::core {

/// Element data types carried by a Tensor (mirrors cvis.v1.DataType, design §4).
enum class DataType {
    kUnspecified,
    kFloat32,
    kFloat16,
    kInt8,
    kUInt8,
    kInt32,
    kInt64,
};

/// Number of bytes occupied by a single element of \p dt. Returns 0 for
/// kUnspecified.
std::size_t byteSize(DataType dt) noexcept;

/// Stable lower-case name for a data type (diagnostics / metrics labels).
const char* toString(DataType dt) noexcept;

} // namespace cvis::core
