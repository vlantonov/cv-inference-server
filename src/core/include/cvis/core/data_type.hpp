#pragma once

/**
 * @file data_type.hpp
 * @brief Tensor element data types and their byte/name helpers.
 * @ingroup cvis_core
 */

#include <cstddef>

namespace cvis::core {

/**
 * @brief Element data types carried by a Tensor.
 *
 * Mirrors `cvis.v1.DataType` on the wire (design §4), so values stay stable
 * across the proto <-> core boundary.
 * @ingroup cvis_core
 */
enum class DataType {
    kUnspecified,
    kFloat32,
    kFloat16,
    kInt8,
    kUInt8,
    kInt32,
    kInt64,
};

/**
 * @brief Number of bytes occupied by a single element of the given type.
 * @param dt Element data type.
 * @return Element size in bytes; 0 for DataType::kUnspecified.
 * @ingroup cvis_core
 */
std::size_t byteSize(DataType dt) noexcept;

/**
 * @brief Stable lower-case name for a data type (diagnostics / metrics labels).
 * @param dt Element data type.
 * @return Null-terminated static string; never nullptr.
 * @ingroup cvis_core
 */
const char* toString(DataType dt) noexcept;

} // namespace cvis::core
