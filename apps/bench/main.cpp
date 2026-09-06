// cvis_bench: benchmark harness (design §8, FR-28..FR-31, NFR-2/NFR-11). Drives
// IInferenceBackend directly (bypassing gRPC) so it measures compute, not
// transport, and links only core/backends/metrics. The CPU path always runs;
// cuda/vulkan cells are marked "unavailable" unless their build flags are on
// (FR-31). JSON is hand-written to keep the harness dependency-free on a bare
// CPU machine (nlohmann_json remains the design's choice when available).
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "cvis/backends/fake_backend.hpp"
#include "cvis/core/core.hpp"

using namespace cvis;

namespace {

using Clock = std::chrono::steady_clock;

struct HardwareInfo {
    std::string cpu_model = "unknown";
    unsigned hw_concurrency = 0;
    std::string gpu = "none";
};

HardwareInfo detectHardware() {
    HardwareInfo hw;
    hw.hw_concurrency = std::thread::hardware_concurrency();
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.rfind("model name", 0) == 0) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                hw.cpu_model = line.substr(pos + 2);
                break;
            }
        }
    }
#ifdef CVIS_ENABLE_CUDA
    hw.gpu = "cuda (enabled at build; runtime-detected on GPU host)";
#endif
    return hw;
}

struct CellResult {
    std::string path;
    std::size_t batch = 0;
    std::size_t concurrency = 0;
    bool available = false;
    double p50_ms = 0.0;
    double p99_ms = 0.0;
    double mean_ms = 0.0;
    double max_ms = 0.0;
    double throughput_ips = 0.0;
    std::uint64_t samples = 0;
};

double percentile(std::vector<double>& v, double pct) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const double rank = pct / 100.0 * static_cast<double>(v.size() - 1);
    const auto lo = static_cast<std::size_t>(rank);
    const auto hi = std::min(lo + 1, v.size() - 1);
    const double frac = rank - static_cast<double>(lo);
    return v[lo] * (1.0 - frac) + v[hi] * frac;
}

core::InferRequest makeRequest(std::size_t idx) {
    core::InferRequest req;
    req.model_id = "bench";
    req.correlation_id = "c" + std::to_string(idx);
    core::Tensor t("input", core::DataType::kFloat32, core::Shape{3, 224, 224});
    req.inputs.push_back(std::move(t));
    return req;
}

// Run one (batch x concurrency) cell against a FakeBackend on CPU.
CellResult runCpuCell(std::size_t batch, std::size_t concurrency,
                      std::size_t iterations, std::size_t warmup) {
    CellResult r;
    r.path = "cpu";
    r.batch = batch;
    r.concurrency = concurrency;
    r.available = true;

    std::vector<std::vector<double>> per_thread(concurrency);
    const auto work = [&](std::size_t tid) {
        backends::FakeBackend backend;
        backends::ModelConfig cfg;
        cfg.model_id = "bench";
        cfg.backend = "fake";
        backend.load(cfg);

        std::vector<core::InferRequest> reqs;
        reqs.reserve(batch);
        for (std::size_t i = 0; i < batch; ++i) reqs.push_back(makeRequest(i));
        std::vector<const core::InferRequest*> ptrs;
        for (const auto& q : reqs) ptrs.push_back(&q);

        for (std::size_t it = 0; it < warmup; ++it) {
            (void)backend.executeBatch({ptrs.data(), ptrs.size()});
        }
        auto& samples = per_thread[tid];
        samples.reserve(iterations);
        for (std::size_t it = 0; it < iterations; ++it) {
            const auto t0 = Clock::now();
            (void)backend.executeBatch({ptrs.data(), ptrs.size()});
            const auto dt = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
            samples.push_back(dt);
        }
    };

    const auto wall0 = Clock::now();
    std::vector<std::thread> threads;
    threads.reserve(concurrency);
    for (std::size_t t = 0; t < concurrency; ++t) threads.emplace_back(work, t);
    for (auto& th : threads) th.join();
    const double wall_s = std::chrono::duration<double>(Clock::now() - wall0).count();

    std::vector<double> all;
    for (auto& v : per_thread) all.insert(all.end(), v.begin(), v.end());
    r.samples = all.size();
    if (!all.empty()) {
        r.mean_ms = std::accumulate(all.begin(), all.end(), 0.0) / static_cast<double>(all.size());
        r.max_ms = *std::max_element(all.begin(), all.end());
        r.p50_ms = percentile(all, 50.0);
        r.p99_ms = percentile(all, 99.0);
    }
    const double total_infers =
        static_cast<double>(iterations) * static_cast<double>(batch) * static_cast<double>(concurrency);
    r.throughput_ips = wall_s > 0 ? total_infers / wall_s : 0.0;
    return r;
}

CellResult unavailableCell(const std::string& path, std::size_t batch, std::size_t concurrency) {
    CellResult r;
    r.path = path;
    r.batch = batch;
    r.concurrency = concurrency;
    r.available = false;
    return r;
}

std::vector<std::size_t> parseList(const char* s, std::vector<std::size_t> fallback) {
    if (s == nullptr) return fallback;
    std::vector<std::size_t> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) out.push_back(static_cast<std::size_t>(std::stoul(tok)));
    }
    return out.empty() ? fallback : out;
}

const char* argValue(int argc, char** argv, const char* flag) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
    }
    return nullptr;
}

void writeJson(std::ostream& os, const HardwareInfo& hw, const std::vector<CellResult>& cells) {
    os << "{\n  \"hardware\": {";
    os << "\"cpu\": \"" << hw.cpu_model << "\", \"hw_concurrency\": " << hw.hw_concurrency
       << ", \"gpu\": \"" << hw.gpu << "\"},\n";
    os << "  \"results\": [\n";
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const auto& c = cells[i];
        os << "    {\"path\": \"" << c.path << "\", \"batch\": " << c.batch
           << ", \"concurrency\": " << c.concurrency
           << ", \"available\": " << (c.available ? "true" : "false")
           << ", \"p50_ms\": " << c.p50_ms << ", \"p99_ms\": " << c.p99_ms
           << ", \"mean_ms\": " << c.mean_ms << ", \"max_ms\": " << c.max_ms
           << ", \"throughput_ips\": " << c.throughput_ips
           << ", \"samples\": " << c.samples << "}";
        os << (i + 1 < cells.size() ? ",\n" : "\n");
    }
    os << "  ]\n}\n";
}

void writeCsv(std::ostream& os, const std::vector<CellResult>& cells) {
    os << "path,batch,concurrency,available,p50_ms,p99_ms,mean_ms,max_ms,throughput_ips,samples\n";
    for (const auto& c : cells) {
        os << c.path << ',' << c.batch << ',' << c.concurrency << ','
           << (c.available ? 1 : 0) << ',' << c.p50_ms << ',' << c.p99_ms << ','
           << c.mean_ms << ',' << c.max_ms << ',' << c.throughput_ips << ',' << c.samples << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    const auto batches = parseList(argValue(argc, argv, "--batch"), {1, 4, 8});
    const auto concs = parseList(argValue(argc, argv, "--concurrency"), {1, 2, 4});
    const std::size_t iters =
        argValue(argc, argv, "--iterations")
            ? static_cast<std::size_t>(std::stoul(argValue(argc, argv, "--iterations")))
            : 200;
    const std::size_t warmup =
        argValue(argc, argv, "--warmup")
            ? static_cast<std::size_t>(std::stoul(argValue(argc, argv, "--warmup")))
            : 20;
    const char* json_path = argValue(argc, argv, "--out");
    const char* csv_path = argValue(argc, argv, "--csv");

    const HardwareInfo hw = detectHardware();
    std::vector<CellResult> cells;

    for (std::size_t b : batches) {
        for (std::size_t c : concs) {
            cells.push_back(runCpuCell(b, c, iters, warmup));
#ifdef CVIS_ENABLE_CUDA
            cells.push_back(unavailableCell("cuda", b, c)); // requires GPU host (R-1)
#else
            cells.push_back(unavailableCell("cuda", b, c));
#endif
#ifdef CVIS_ENABLE_VULKAN_BENCH
            cells.push_back(unavailableCell("vulkan", b, c)); // best-effort stub (R-5)
#else
            cells.push_back(unavailableCell("vulkan", b, c));
#endif
        }
    }

    writeJson(std::cout, hw, cells);
    if (json_path) {
        std::ofstream f(json_path);
        writeJson(f, hw, cells);
        std::cerr << "wrote " << json_path << "\n";
    }
    if (csv_path) {
        std::ofstream f(csv_path);
        writeCsv(f, cells);
        std::cerr << "wrote " << csv_path << "\n";
    }
    return 0;
}
