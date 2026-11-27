/**
 * @file main.cpp
 * @brief `cvis-server` executable: thin composition entrypoint.
 * @ingroup cvis_server
 *
 * Loads configuration, builds a ServerApp, installs SIGINT/SIGTERM handlers for
 * a graceful drain, and runs until shutdown (design §6).
 */
#include <atomic>
#include <csignal>
#include <iostream>

#include "cvis/runtime/config.hpp"
#include "cvis/runtime/server_app.hpp"

namespace {
std::atomic<cvis::runtime::ServerApp*> g_app{nullptr};

void handleSignal(int) {
    if (auto* app = g_app.load()) {
        app->requestShutdown();
    }
}
} // namespace

/**
 * @brief Process entrypoint: wire configuration + ServerApp and run to shutdown.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on clean shutdown; 1 on config, startup, or runtime error.
 * @ingroup cvis_server
 */
int main(int argc, char** argv) {
    auto cfg = cvis::runtime::loadConfig(argc, argv);
    if (!cfg) {
        std::cerr << "config error: " << cfg.error().message << "\n";
        return 1;
    }

    auto app = cvis::runtime::ServerApp::create(std::move(cfg).value());
    if (!app) {
        std::cerr << "startup error: " << app.error().message << "\n";
        return 1;
    }

    cvis::runtime::ServerApp& server = app.value();
    g_app.store(&server);
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    const cvis::core::Status s = server.run();
    g_app.store(nullptr);
    if (!s.ok()) {
        std::cerr << "runtime error: " << s.message << "\n";
        return 1;
    }
    return 0;
}
