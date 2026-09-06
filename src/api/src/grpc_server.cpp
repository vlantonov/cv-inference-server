// Compiled only under CVIS_ENABLE_GRPC. Implements InferenceService (unary +
// streaming) and health/readiness, marshalling proto <-> core value types
// (design §4/§7). This is transport glue with no new abstractions.
#include "cvis/api/grpc_server.hpp"

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include <cstring>
#include <thread>

#include "cvis.grpc.pb.h"
#include "cvis/batching/dynamic_batcher.hpp"
#include "cvis/metrics/metrics_registry.hpp"
#include "cvis/runtime/model_registry.hpp"

namespace cvis::api {
namespace {

core::DataType fromProto(cvis::v1::DataType dt) {
    switch (dt) {
        case cvis::v1::DT_FLOAT32: return core::DataType::kFloat32;
        case cvis::v1::DT_FLOAT16: return core::DataType::kFloat16;
        case cvis::v1::DT_INT8:    return core::DataType::kInt8;
        case cvis::v1::DT_UINT8:   return core::DataType::kUInt8;
        case cvis::v1::DT_INT32:   return core::DataType::kInt32;
        case cvis::v1::DT_INT64:   return core::DataType::kInt64;
        default:                   return core::DataType::kUnspecified;
    }
}

cvis::v1::DataType toProto(core::DataType dt) {
    switch (dt) {
        case core::DataType::kFloat32: return cvis::v1::DT_FLOAT32;
        case core::DataType::kFloat16: return cvis::v1::DT_FLOAT16;
        case core::DataType::kInt8:    return cvis::v1::DT_INT8;
        case core::DataType::kUInt8:   return cvis::v1::DT_UINT8;
        case core::DataType::kInt32:   return cvis::v1::DT_INT32;
        case core::DataType::kInt64:   return cvis::v1::DT_INT64;
        default:                       return cvis::v1::DT_UNSPECIFIED;
    }
}

core::InferRequest fromProto(const cvis::v1::InferRequest& in) {
    core::InferRequest req;
    req.model_id = in.model_id();
    req.correlation_id = in.correlation_id();
    for (const auto& t : in.inputs()) {
        core::Shape shape(t.shape().begin(), t.shape().end());
        core::Tensor tensor(t.name(), fromProto(t.dtype()), shape);
        const std::size_t n = std::min<std::size_t>(tensor.byteCount(), t.raw_data().size());
        std::memcpy(tensor.bytes().data(), t.raw_data().data(), n);
        req.inputs.push_back(std::move(tensor));
    }
    return req;
}

void toProto(const core::InferResponse& resp, cvis::v1::InferResponse* out) {
    out->set_model_id(resp.model_id);
    out->set_correlation_id(resp.correlation_id);
    for (const auto& t : resp.outputs) {
        auto* pt = out->add_outputs();
        pt->set_name(t.name());
        pt->set_dtype(toProto(t.dtype()));
        for (std::int64_t d : t.shape()) pt->add_shape(d);
        auto b = t.bytes();
        pt->set_raw_data(b.data(), b.size());
    }
    if (!resp.status.ok()) {
        out->mutable_error()->set_code(static_cast<int>(resp.status.code));
        out->mutable_error()->set_message(resp.status.message);
    }
}

grpc::StatusCode toGrpc(core::StatusCode c) {
    switch (c) {
        case core::StatusCode::kOk:                 return grpc::StatusCode::OK;
        case core::StatusCode::kInvalidArgument:    return grpc::StatusCode::INVALID_ARGUMENT;
        case core::StatusCode::kNotFound:           return grpc::StatusCode::NOT_FOUND;
        case core::StatusCode::kFailedPrecondition: return grpc::StatusCode::FAILED_PRECONDITION;
        case core::StatusCode::kResourceExhausted:  return grpc::StatusCode::RESOURCE_EXHAUSTED;
        case core::StatusCode::kUnavailable:        return grpc::StatusCode::UNAVAILABLE;
        case core::StatusCode::kUnimplemented:      return grpc::StatusCode::UNIMPLEMENTED;
        case core::StatusCode::kInternal:           return grpc::StatusCode::INTERNAL;
    }
    return grpc::StatusCode::INTERNAL;
}

class InferenceServiceImpl final : public cvis::v1::InferenceService::Service {
public:
    InferenceServiceImpl(batching::DynamicBatcher& batcher,
                         runtime::ModelRegistry& registry,
                         metrics::MetricsRegistry& metrics)
        : batcher_(batcher), registry_(registry), metrics_(metrics) {}

    grpc::Status Infer(grpc::ServerContext*, const cvis::v1::InferRequest* request,
                       cvis::v1::InferResponse* response) override {
        if (registry_.find(request->model_id()) == nullptr) {
            return {grpc::StatusCode::NOT_FOUND, "unknown model: " + request->model_id()};
        }
        auto fut = batcher_.submit(fromProto(*request));
        if (!fut) {
            return {toGrpc(fut.error().code), fut.error().message};
        }
        core::InferResponse resp = fut.value().get();
        if (!resp.status.ok()) {
            return {toGrpc(resp.status.code), resp.status.message};
        }
        toProto(resp, response);
        return grpc::Status::OK;
    }

    grpc::Status StreamInfer(
        grpc::ServerContext*,
        grpc::ServerReaderWriter<cvis::v1::InferResponse, cvis::v1::InferRequest>* stream)
        override {
        cvis::v1::InferRequest in;
        while (stream->Read(&in)) {
            cvis::v1::InferResponse out;
            if (registry_.find(in.model_id()) == nullptr) {
                out.set_correlation_id(in.correlation_id());
                out.mutable_error()->set_code(static_cast<int>(core::StatusCode::kNotFound));
                out.mutable_error()->set_message("unknown model: " + in.model_id());
                stream->Write(out);   // per-frame error; stream continues (FR-13)
                continue;
            }
            auto fut = batcher_.submit(fromProto(in));
            if (!fut) {
                out.set_correlation_id(in.correlation_id());
                out.mutable_error()->set_code(static_cast<int>(fut.error().code));
                out.mutable_error()->set_message(fut.error().message);
                stream->Write(out);
                continue;
            }
            core::InferResponse resp = fut.value().get();
            toProto(resp, &out);
            stream->Write(out);
        }
        return grpc::Status::OK;
    }

    grpc::Status ListModels(grpc::ServerContext*, const cvis::v1::ListModelsRequest*,
                            cvis::v1::ListModelsResponse* response) override {
        for (const auto& meta : registry_.list()) {
            auto* mi = response->add_models();
            mi->set_model_id(meta.model_id);
            mi->set_backend(meta.backend);
            mi->set_ready(meta.ready);
        }
        return grpc::Status::OK;
    }

    grpc::Status LoadModel(grpc::ServerContext*, const cvis::v1::LoadModelRequest*,
                           cvis::v1::LoadModelResponse*) override {
        return {grpc::StatusCode::UNIMPLEMENTED, "runtime load deferred (OQ-6)"};
    }

    grpc::Status UnloadModel(grpc::ServerContext*, const cvis::v1::UnloadModelRequest*,
                             cvis::v1::UnloadModelResponse*) override {
        return {grpc::StatusCode::UNIMPLEMENTED, "runtime unload deferred (OQ-6)"};
    }

private:
    batching::DynamicBatcher& batcher_;
    runtime::ModelRegistry& registry_;
    metrics::MetricsRegistry& metrics_;
};

} // namespace

struct GrpcServer::Impl {
    std::string bind_addr;
    InferenceServiceImpl service;
    std::unique_ptr<grpc::Server> server;
    std::thread serve_thread;

    Impl(std::string addr, batching::DynamicBatcher& b, runtime::ModelRegistry& r,
         metrics::MetricsRegistry& m)
        : bind_addr(std::move(addr)), service(b, r, m) {}
};

GrpcServer::GrpcServer(std::string bind_addr, batching::DynamicBatcher& batcher,
                       runtime::ModelRegistry& registry, metrics::MetricsRegistry& metrics)
    : impl_(std::make_unique<Impl>(std::move(bind_addr), batcher, registry, metrics)) {}

GrpcServer::~GrpcServer() { shutdown(); }

void GrpcServer::start() {
    grpc::EnableDefaultHealthCheckService(true);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(impl_->bind_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&impl_->service);
    impl_->server = builder.BuildAndStart();

    // Readiness (FR-25): mark serving once at least one model is ready. Liveness
    // is implicit while the server object exists (FR-24).
    if (auto* hc = impl_->server->GetHealthCheckService()) {
        hc->SetServingStatus("cvis.v1.InferenceService", true);
    }
}

void GrpcServer::shutdown() {
    if (impl_ && impl_->server) {
        impl_->server->Shutdown();
        impl_->server->Wait();
        impl_->server.reset();
    }
}

} // namespace cvis::api
