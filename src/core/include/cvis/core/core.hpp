#pragma once

/**
 * @file core.hpp
 * @brief Umbrella header pulling in every `cvis::core` value type.
 * @ingroup cvis_core
 *
 * Convenience include for consumers that want the whole core vocabulary
 * (Tensor, DataType, Status, Expected, InferRequest/Response) in one line
 * (design interfaces §1).
 */

// Umbrella header for the cvis::core value types (design interfaces §1).
#include "cvis/core/data_type.hpp"
#include "cvis/core/expected.hpp"
#include "cvis/core/infer.hpp"
#include "cvis/core/status.hpp"
#include "cvis/core/tensor.hpp"
