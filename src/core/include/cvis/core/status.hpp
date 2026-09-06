#pragma once

#include <string>

namespace cvis::core {

/// Canonical status codes carried across module boundaries. Maps 1:1 onto the
/// gRPC status codes used by the api layer (design §4/§9).
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

/// Lower-case, stable string for a status code.
const char* toString(StatusCode code) noexcept;

/// Value-type status returned from fallible operations that carry no payload.
struct Status {
    StatusCode code = StatusCode::kOk;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return code == StatusCode::kOk; }

    static Status Ok() { return {}; }

    static Status Error(StatusCode c, std::string msg) {
        return Status{c, std::move(msg)};
    }
};

} // namespace cvis::core
