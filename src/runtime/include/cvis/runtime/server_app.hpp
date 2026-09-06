#pragma once

#include <memory>

#include "cvis/core/expected.hpp"
#include "cvis/core/status.hpp"
#include "cvis/runtime/config.hpp"

namespace cvis::runtime {

/// Composition root. Owns all long-lived objects (ModelRegistry -> backends,
/// Scheduler, DynamicBatcher, MetricsRegistry, and -- when built with gRPC --
/// the gRPC server). RAII teardown in reverse construction order (design §6,
/// NFR-5). Startup errors surface as a clear diagnostic + non-zero exit (NFR-13).
class ServerApp {
public:
    static core::Expected<ServerApp> create(Config cfg);

    ServerApp(ServerApp&&) noexcept;
    ServerApp& operator=(ServerApp&&) noexcept;
    ~ServerApp();

    ServerApp(const ServerApp&) = delete;
    ServerApp& operator=(const ServerApp&) = delete;

    /// Blocks until a shutdown is requested (signal or requestShutdown()).
    core::Status run();

    /// Graceful drain; safe to call from a signal handler / another thread.
    void requestShutdown() noexcept;

private:
    class Impl;
    explicit ServerApp(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace cvis::runtime
