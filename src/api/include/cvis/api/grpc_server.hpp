#pragma once

/**
 * @file grpc_server.hpp
 * @brief RAII wrapper around the gRPC server hosting InferenceService + Health.
 * @ingroup cvis_api
 */

// Compiled only under CVIS_ENABLE_GRPC. Thin RAII wrapper around the gRPC server
// hosting InferenceService + the standard grpc.health.v1.Health service
// (design §4/§7). Marshals proto <-> core value types and calls DynamicBatcher.
#include <memory>
#include <string>

namespace cvis::batching { class DynamicBatcher; }
namespace cvis::metrics { class MetricsRegistry; }
namespace cvis::runtime { class ModelRegistry; }

namespace cvis::api {

/**
 * @brief Thin RAII wrapper around the gRPC server hosting `InferenceService`
 *        (unary + streaming) and the standard `grpc.health.v1.Health` service.
 *
 * Marshals proto <-> core value types and forwards requests to the
 * DynamicBatcher (design §4/§7).
 * @note Compiled only under `CVIS_ENABLE_GRPC`; excluded from the default
 *       CPU-only build when that option is off.
 * @ingroup cvis_api
 */
class GrpcServer {
public:
    /**
     * @brief Construct the server wiring (does not start listening).
     * @param bind_addr Listen address (e.g. "0.0.0.0:50051").
     * @param batcher   Front door requests are submitted to.
     * @param registry  Model registry queried for readiness/metadata.
     * @param metrics   Registry for request metrics.
     */
    GrpcServer(std::string bind_addr,
               batching::DynamicBatcher& batcher,
               runtime::ModelRegistry& registry,
               metrics::MetricsRegistry& metrics);
    ~GrpcServer();

    GrpcServer(const GrpcServer&) = delete;
    GrpcServer& operator=(const GrpcServer&) = delete;

    void start();      ///< non-blocking: builds and starts the server
    void shutdown();   ///< graceful drain + stop

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cvis::api
