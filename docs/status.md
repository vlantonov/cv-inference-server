# Project status — cv-inference-server

- **Project:** `cv-inference-server`
- **Doc type:** Running project status (PM-facing ground truth)
- **CMake version:** 0.1.0 (release tag applied separately by the PM via the
  semver-version-publish skill)
- **Last updated:** 2026-09-07 (Documentation stage)

This is the concise, authoritative summary of *what is actually implemented and
validated* versus designed-but-gated versus deferred. It exists so the next
planning pass sees accurate ground truth. It is consistent with the code and the
gating reality as of this update.

---

## One-line summary

A **CPU vertical slice** has shipped: the dependency-free core (batching,
scheduling, fake backend, metrics) builds green and is fully tested on a bare CPU
host; the gRPC / ONNX Runtime / TensorRT / Vulkan layers are designed, wired, and
auto/flag-gated OFF, validated only where their deps or GPU hardware are present.

---

## Capability status by phase

| Phase | Scope | Status |
|---|---|---|
| **1–3** | Core value types, backend abstraction + `FakeBackend`, dynamic batching, fair scheduling, metrics registry + Prometheus text | ✅ **Implemented & tested** on CPU (34 GoogleTest tests, ASan + TSan clean, in CI). |
| **4** | GPU/accelerated serving: gRPC transport, ONNX Runtime (CPU + CUDA EP), TensorRT backend, HTTP `/metrics`, YAML/CLI config | ⚙️ **Code present, gated.** Auto-gated by dependency detection or opt-in flags; compiled and exercised only where deps/GPU exist — **not** in CPU CI. |
| **5** | Benchmark harness | ✅ CPU path **runnable everywhere** (batch × concurrency sweep, p50/p99/mean/max + throughput, JSON + CSV, reproducibility metadata). ⚙️ CUDA cells GPU-only; Vulkan best-effort/stub. |

---

## Detailed status

### Implemented & CI-verified (CPU)
- `cvis_core`, `cvis_backends` (`FakeBackend` + factory), `cvis_batching`,
  `cvis_scheduler`, `cvis_metrics`.
- `cvis-server` core pipeline boot (batcher + scheduler + registry) with a default
  `echo`/`fake` model and graceful shutdown.
- `cvis-bench` CPU sweep with JSON/CSV output.
- 34 GoogleTest unit tests; ASan/UBSan + TSan matrix; CPack TGZ packaging;
  release-on-tag workflow (nothing published yet).
- CI on GitHub Actions **and** GitLab CI (build+test + sanitizer matrix).

### Present but gated / manually validated
- gRPC transport (`cvis_api`: unary + streaming, health/readiness) — `CVIS_ENABLE_GRPC`.
- ONNX Runtime backend — `CVIS_ENABLE_ONNXRUNTIME` (+ `CVIS_ENABLE_CUDA` for GPU EP).
- TensorRT backend — `CVIS_ENABLE_TENSORRT` (implies CUDA), GPU host only.
- Prometheus HTTP `/metrics` exposer — `CVIS_ENABLE_METRICS_HTTP`.
- Full YAML + CLI11 config — `CVIS_ENABLE_CONFIG_YAML`.
- Vulkan benchmark compute path — `CVIS_ENABLE_VULKAN_BENCH` (stub).

---

## Open follow-ups

| Item | Reference | Notes |
|---|---|---|
| Automated GPU CI (CUDA/TensorRT build + bench) | R-1 | Needs GPU runners; currently manual on GPU hosts. |
| Integration coverage for gRPC + ONNX Runtime when deps present | — | Current tests are unit-level against `FakeBackend`; add end-to-end tests once the gated layers are built in a deps-available job. |
| Runtime model load/unload management RPCs | FR-5 / OQ-6 | Deferred; startup-time load is the v1 requirement. Proto methods exist, may return `UNIMPLEMENTED`. |
| Vulkan benchmark compute path | OQ-1 / R-5 | Best-effort, benchmark-only; currently a gated stub. |
| Empirical GPU latency baselining | NFR-1 / OQ-2 | Fill in real p50/p99 numbers once run on GPU hardware. |

---

## Documentation TODOs (fill in on hardware)
- GPU benchmark numbers (CUDA/TensorRT p50/p99/throughput) once run on a GPU host —
  currently the bench emits `unavailable` cells for those paths on CPU.
- End-to-end gRPC usage example (client snippet) once a deps-available build is
  exercised and captured.
