#include "cvis/runtime/server_app.hpp"

#include <condition_variable>
#include <iostream>
#include <mutex>

#include "cvis/backends/backend.hpp"
#include "cvis/batching/dynamic_batcher.hpp"
#include "cvis/metrics/metrics_http_exposer.hpp"
#include "cvis/metrics/metrics_registry.hpp"
#include "cvis/runtime/model_registry.hpp"
#include "cvis/scheduler/scheduler.hpp"

#ifdef CVIS_ENABLE_GRPC
#include "cvis/api/grpc_server.hpp"
#endif

namespace cvis::runtime {

class ServerApp::Impl {
public:
    explicit Impl(Config cfg) : cfg_(std::move(cfg)) {}

    core::Status init() {
        factory_.registerBuiltins();
        if (core::Status s = registry_.loadFromConfig(cfg_, factory_); !s.ok()) {
            return s;
        }
        if (!registry_.anyReady()) {
            return core::Status::Error(core::StatusCode::kFailedPrecondition,
                                       "no model became ready at startup");
        }

        scheduler_ = std::make_unique<scheduler::Scheduler>(cfg_.scheduler, metrics_);
        for (const auto& meta : registry_.list()) {
            if (auto* backend = registry_.find(meta.model_id)) {
                scheduler_->registerModel(meta.model_id, *backend);
            }
        }
        batcher_ = std::make_unique<batching::DynamicBatcher>(cfg_.batching, *scheduler_, metrics_);
        exposer_ = std::make_unique<metrics::MetricsHttpExposer>(
            metrics_, cfg_.metrics_bind, cfg_.metrics_port);

#ifdef CVIS_ENABLE_GRPC
        grpc_ = std::make_unique<api::GrpcServer>(cfg_.grpc_bind, *batcher_, registry_, metrics_);
#endif
        return core::Status::Ok();
    }

    core::Status run() {
#ifdef CVIS_ENABLE_GRPC
        std::cout << "cvis: serving gRPC on " << cfg_.grpc_bind << "\n";
        grpc_->start();
#else
        std::cout << "cvis: gRPC transport not compiled (CVIS_ENABLE_GRPC=OFF); "
                     "core pipeline is up. Models ready: "
                  << registry_.list().size() << "\n";
#endif
        if (exposer_->serving()) {
            std::cout << "cvis: metrics on " << cfg_.metrics_bind << ":" << cfg_.metrics_port
                      << "/metrics\n";
        }

        std::unique_lock lk(mu_);
        cv_.wait(lk, [this] { return stop_; });
        lk.unlock();

#ifdef CVIS_ENABLE_GRPC
        grpc_->shutdown();
#endif
        if (batcher_) batcher_->shutdown();
        if (scheduler_) scheduler_->shutdown();
        return core::Status::Ok();
    }

    void requestShutdown() noexcept {
        {
            std::lock_guard lk(mu_);
            stop_ = true;
        }
        cv_.notify_all();
    }

private:
    Config cfg_;
    metrics::MetricsRegistry metrics_;
    backends::BackendFactory factory_;
    ModelRegistry registry_;
    std::unique_ptr<scheduler::Scheduler> scheduler_;
    std::unique_ptr<batching::DynamicBatcher> batcher_;
    std::unique_ptr<metrics::MetricsHttpExposer> exposer_;
#ifdef CVIS_ENABLE_GRPC
    std::unique_ptr<api::GrpcServer> grpc_;
#endif

    std::mutex mu_;
    std::condition_variable cv_;
    bool stop_ = false;
};

ServerApp::ServerApp(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ServerApp::ServerApp(ServerApp&&) noexcept = default;
ServerApp& ServerApp::operator=(ServerApp&&) noexcept = default;
ServerApp::~ServerApp() = default;

core::Expected<ServerApp> ServerApp::create(Config cfg) {
    auto impl = std::make_unique<Impl>(std::move(cfg));
    if (core::Status s = impl->init(); !s.ok()) {
        return s;
    }
    return ServerApp(std::move(impl));
}

core::Status ServerApp::run() { return impl_->run(); }

void ServerApp::requestShutdown() noexcept {
    if (impl_) impl_->requestShutdown();
}

} // namespace cvis::runtime
