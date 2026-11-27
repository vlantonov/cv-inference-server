#pragma once

/**
 * @file status.hpp
 * @brief Canonical status codes and the payload-free Status value type.
 * @ingroup cvis_core
 */

#include <string>

namespace cvis::core {

/**
 * @brief Canonical status codes carried across module boundaries.
 *
 * Maps 1:1 onto the gRPC status codes used by the api layer (design §4/§9).
 * @ingroup cvis_core
 */
enum class StatusCode {
    kOk,
    kInvalidArgument,
    kNotFound,
    kFailedPrecondition,
    kResourceExhausted,
    kUnavailable,
    kInternal,
    kUnimplemented,
};

/**
 * @brief Lower-case, stable string for a status code (diagnostics / labels).
 * @param code Status code.
 * @return Null-terminated static string; never nullptr.
 */
const char* toString(StatusCode code) noexcept;

/**
 * @brief Value-type status returned from fallible operations that carry no
 *        payload.
 * @ingroup cvis_core
 */
struct Status {
    StatusCode code = StatusCode::kOk;
    std::string message;

    /// @brief True when the status represents success (StatusCode::kOk).
    [[nodiscard]] bool ok() const noexcept { return code == StatusCode::kOk; }

    /// @brief Construct a success status.
    static Status Ok() { return {}; }

    /// @brief Construct a failure status with code @p c and diagnostic @p msg.
    static Status Error(StatusCode c, std::string msg) {
        return Status{c, std::move(msg)};
    }
};

} // namespace cvis::core
