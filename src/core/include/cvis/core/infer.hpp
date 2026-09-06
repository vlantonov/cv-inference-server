#pragma once

#include <string>
#include <vector>

#include "cvis/core/status.hpp"
#include "cvis/core/tensor.hpp"

namespace cvis::core {

/// One logical inference unit flowing through the pipeline. Owning value type,
/// moved (never copied) between layers (design interfaces §1).
struct InferRequest {
    std::string model_id;
    std::string correlation_id;   ///< echoed back on the response (FR-12/FR-14)
    std::vector<Tensor> inputs;
};

/// Result for a single request, index-aligned with the originating request when
/// demuxed out of a batch (design §7.2, FR-18).
struct InferResponse {
    std::string model_id;
    std::string correlation_id;
    std::vector<Tensor> outputs;
    Status status;                ///< per-item status (streaming, FR-13/FR-14)
};

} // namespace cvis::core
