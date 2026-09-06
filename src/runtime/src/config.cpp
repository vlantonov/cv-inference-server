#include "cvis/runtime/config.hpp"

#include <cstring>
#include <string>

#ifdef CVIS_ENABLE_CONFIG_YAML
#include <CLI/CLI.hpp>
#include <yaml-cpp/yaml.h>
#endif

namespace cvis::runtime {

Config defaultConfig() {
    Config cfg;
    backends::ModelConfig echo;
    echo.model_id = "echo";
    echo.backend = "fake";
    cfg.models.push_back(std::move(echo));
    return cfg;
}

namespace {

// Minimal argv scan used by both paths for the handful of overrides that must
// work even without CLI11 (design OQ-5: CLI overrides file/defaults).
const char* argValue(int argc, char** argv, const char* flag) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
    }
    return nullptr;
}

} // namespace

#ifdef CVIS_ENABLE_CONFIG_YAML

core::Expected<Config> loadConfig(int argc, char** argv) {
    std::string config_path;
    std::string grpc_bind;
    std::string metrics_bind;
    int metrics_port = -1;

    CLI::App app{"cv-inference-server"};
    app.add_option("-c,--config", config_path, "YAML config file");
    app.add_option("--grpc-bind", grpc_bind, "gRPC bind address host:port");
    app.add_option("--metrics-bind", metrics_bind, "metrics bind host");
    app.add_option("--metrics-port", metrics_port, "metrics port");
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return core::Status::Error(core::StatusCode::kInvalidArgument,
                                   std::string("CLI parse error: ") + e.what());
    }

    Config cfg = defaultConfig();
    if (!config_path.empty()) {
        try {
            YAML::Node root = YAML::LoadFile(config_path);
            if (root["grpc_bind"]) cfg.grpc_bind = root["grpc_bind"].as<std::string>();
            if (root["metrics_bind"]) cfg.metrics_bind = root["metrics_bind"].as<std::string>();
            if (root["metrics_port"]) cfg.metrics_port = root["metrics_port"].as<std::uint16_t>();
            if (root["batching"]) {
                auto b = root["batching"];
                if (b["enabled"]) cfg.batching.enabled = b["enabled"].as<bool>();
                if (b["max_batch_size"]) cfg.batching.max_batch_size = b["max_batch_size"].as<std::size_t>();
                if (b["max_wait_ms"]) cfg.batching.max_wait = std::chrono::milliseconds(b["max_wait_ms"].as<int>());
                if (b["max_queue_depth"]) cfg.batching.max_queue_depth = b["max_queue_depth"].as<std::size_t>();
            }
            if (root["scheduler"]) {
                auto s = root["scheduler"];
                if (s["workers"]) cfg.scheduler.workers = s["workers"].as<std::size_t>();
                if (s["max_in_flight"]) cfg.scheduler.max_in_flight = s["max_in_flight"].as<std::size_t>();
            }
            if (root["models"] && root["models"].IsSequence()) {
                cfg.models.clear();
                for (const auto& node : root["models"]) {
                    backends::ModelConfig mc;
                    mc.model_id = node["id"].as<std::string>();
                    mc.backend = node["backend"] ? node["backend"].as<std::string>() : "onnxruntime";
                    if (node["path"]) mc.path = node["path"].as<std::string>();
                    if (node["use_gpu"]) mc.use_gpu = node["use_gpu"].as<bool>();
                    cfg.models.push_back(std::move(mc));
                }
            }
        } catch (const YAML::Exception& e) {
            return core::Status::Error(core::StatusCode::kInvalidArgument,
                                       std::string("YAML config error: ") + e.what());
        }
    }

    if (!grpc_bind.empty()) cfg.grpc_bind = grpc_bind;
    if (!metrics_bind.empty()) cfg.metrics_bind = metrics_bind;
    if (metrics_port >= 0) cfg.metrics_port = static_cast<std::uint16_t>(metrics_port);
    return cfg;
}

#else // no YAML/CLI dependency compiled in -> defaults + minimal argv overrides

core::Expected<Config> loadConfig(int argc, char** argv) {
    Config cfg = defaultConfig();
    if (const char* v = argValue(argc, argv, "--grpc-bind")) cfg.grpc_bind = v;
    if (const char* v = argValue(argc, argv, "--metrics-bind")) cfg.metrics_bind = v;
    if (const char* v = argValue(argc, argv, "--metrics-port")) {
        cfg.metrics_port = static_cast<std::uint16_t>(std::stoi(v));
    }
    return cfg;
}

#endif

} // namespace cvis::runtime
