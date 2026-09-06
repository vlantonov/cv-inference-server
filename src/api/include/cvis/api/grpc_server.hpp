#pragma once

// Compiled only under CVIS_ENABLE_GRPC. Thin RAII wrapper around the gRPC server
// hosting InferenceService + the standard grpc.health.v1.Health service
// (design §4/§7). Marshals proto <-> core value types and calls DynamicBatcher.
#include <memory>
#include <string>

namespace cvis::batching { class DynamicBatcher; }
namespace cvis::metrics { class MetricsRegistry; }
namespace cvis::runtime { class ModelRegistry; }

namespace cvis::api {

class GrpcServer {
public:
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
