#pragma once

/**
 * @file config.hpp
 * @brief Fully-resolved server Config plus its loader and CPU-default factory.
 * @ingroup cvis_runtime
 */

#include <cstdint>
#include <string>
#include <vector>

#include "cvis/backends/backend.hpp"
#include "cvis/batching/dynamic_batcher.hpp"
#include "cvis/core/expected.hpp"
#include "cvis/scheduler/scheduler.hpp"

namespace cvis::runtime {

/**
 * @brief Fully-resolved server configuration (design interfaces §6).
 *
 * Populated from a YAML file with CLI overrides (OQ-5); when the YAML/CLI
 * dependencies are not compiled in, a sensible CPU default with a single "fake"
 * model is returned so the server still starts (NFR-8).
 * @ingroup cvis_runtime
 */
struct Config {
    std::string grpc_bind = "0.0.0.0:50051";
    std::string metrics_bind = "0.0.0.0";
    std::uint16_t metrics_port = 9090;
    std::vector<backends::ModelConfig> models;
    batching::BatchingConfig batching;
    scheduler::SchedulerConfig scheduler;
};

/**
 * @brief Load configuration from CLI args (and a YAML file when built with
 *        `CVIS_ENABLE_CONFIG_YAML`).
 *
 * CLI flags override file values (OQ-5).
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return The resolved Config, or a Status describing a parse/validation error.
 * @ingroup cvis_runtime
 */
core::Expected<Config> loadConfig(int argc, char** argv);

/**
 * @brief Return a minimal CPU-only default configuration (one echo model).
 * @ingroup cvis_runtime
 */
Config defaultConfig();

} // namespace cvis::runtime
