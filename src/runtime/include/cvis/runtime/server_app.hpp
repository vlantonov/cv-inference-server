#pragma once

/**
 * @file server_app.hpp
 * @brief ServerApp composition root owning the full server lifecycle.
 * @ingroup cvis_runtime
 */

#include <memory>

#include "cvis/core/expected.hpp"
#include "cvis/core/status.hpp"
#include "cvis/runtime/config.hpp"

namespace cvis::runtime {

/**
 * @brief Composition root owning all long-lived objects.
 *
 * Owns the ModelRegistry (-> backends), Scheduler, DynamicBatcher,
 * MetricsRegistry, and -- when built with gRPC -- the gRPC server. RAII teardown
 * happens in reverse construction order (design §6, NFR-5). Startup errors
 * surface as a clear diagnostic + non-zero exit (NFR-13).
 * @ingroup cvis_runtime
 */
class ServerApp {
public:
    /**
     * @brief Build and wire the whole server from @p cfg.
     * @param cfg Fully-resolved configuration.
     * @return A ready ServerApp, or a Status describing the startup failure.
     */
    static core::Expected<ServerApp> create(Config cfg);

    ServerApp(ServerApp&&) noexcept;
    ServerApp& operator=(ServerApp&&) noexcept;
    ~ServerApp();

    ServerApp(const ServerApp&) = delete;
    ServerApp& operator=(const ServerApp&) = delete;

    /// @brief Block until a shutdown is requested (signal or requestShutdown()).
    core::Status run();

    /// @brief Request a graceful drain; safe from a signal handler / other thread.
    void requestShutdown() noexcept;

private:
    class Impl;
    explicit ServerApp(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace cvis::runtime
