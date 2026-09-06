#include "cvis/core/status.hpp"

namespace cvis::core {

const char* toString(StatusCode code) noexcept {
    switch (code) {
        case StatusCode::kOk:                 return "ok";
        case StatusCode::kInvalidArgument:    return "invalid_argument";
        case StatusCode::kNotFound:           return "not_found";
        case StatusCode::kFailedPrecondition: return "failed_precondition";
        case StatusCode::kResourceExhausted:  return "resource_exhausted";
        case StatusCode::kUnavailable:        return "unavailable";
        case StatusCode::kInternal:           return "internal";
        case StatusCode::kUnimplemented:      return "unimplemented";
    }
    return "internal";
}

} // namespace cvis::core
