# Changelog

All notable changes to this project are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres
to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!--
The version/date heading below is finalized by the Project Manager via the
semver-version-publish skill when the first release tag is cut. Until then this
section describes the first release contents.
-->

## [Unreleased] — first release

Initial implementation: a CPU vertical slice of a real-time CV model serving
system, with the GPU/accelerated layers designed and wired but gated OFF by
default.

### Added

- **Requirements & design docs.** SRS (`docs/requirements/SRS.md`) and the design
  specification with interface signatures and Mermaid data-flow
  (`docs/design/architecture.md`, `docs/design/interfaces.md`).
- **Core (`cvis_core`).** Value types — `Tensor`/`TensorView`, `DataType`, `Shape`,
  `InferRequest`/`InferResponse`, `Status`, and an `Expected` error channel.
  Move-only, RAII tensors; no I/O, no GPU.
- **Backends (`cvis_backends`).** `IInferenceBackend` abstraction with a
  factory; `FakeBackend` (echo) always built. `OnnxRuntimeBackend` (CPU EP always,
  CUDA EP under CUDA) and `TensorRtBackend` present but compiled only when their
  deps/flags are enabled; unknown/unavailable backends fail gracefully with a
  reason.
- **Dynamic batching (`cvis_batching`).** `DynamicBatcher` coalesces per model,
  flushes on **max batch size OR max-wait deadline**, demultiplexes results back to
  each correlation id, supports a disable (batch-of-one) path, and applies
  bounded-queue backpressure (`RESOURCE_EXHAUSTED`).
- **Scheduling (`cvis_scheduler`).** `Scheduler` with round-robin fairness
  (no starvation), bounded in-flight batches, worker pool, and error propagation.
- **Metrics (`cvis_metrics`).** `MetricsRegistry` (counters/gauges/histograms)
  with Prometheus text exposition; HTTP `/metrics` exposer gated behind
  prometheus-cpp.
- **gRPC API (`cvis_api`).** Unary `Infer` + streaming `StreamInfer` and
  health/readiness, marshalling proto ↔ core. Gated behind `CVIS_ENABLE_GRPC`
  (protobuf + gRPC). Management `LoadModel`/`UnloadModel` present in the proto
  contract but deferred (may return `UNIMPLEMENTED`) per FR-5 / OQ-6.
- **Runtime (`cvis_runtime`).** `Config` loader (full YAML + CLI11 when available,
  with a dependency-free argv/default fallback), `ModelRegistry`, and `ServerApp`
  lifecycle with startup model load and graceful `SIGINT`/`SIGTERM` shutdown.
- **Executables.** `cvis-server` (composition root) and `cvis-bench`.
- **Benchmark harness.** `cvis-bench` sweeps batch × concurrency and reports
  p50/p99/mean/max latency + throughput as JSON (stdout + file) and CSV, with
  hardware/reproducibility metadata. CPU path runs everywhere; `cuda`/`vulkan`
  cells are marked `unavailable` without the corresponding build flag + GPU host.
- **Quality.** 34 GoogleTest unit tests; clean AddressSanitizer/UBSan and
  ThreadSanitizer runs.
- **CI/CD & packaging.** GitHub Actions and GitLab CI pipelines (CPU Release
  build + full test suite, plus an ASan/TSan sanitizer matrix); CPack TGZ
  packaging and a release-on-tag workflow that builds, tests, packages, and
  attaches the tarball to the GitHub Release. Nothing has been published yet.

### Fixed

- **D-1 (sanitizer/CI stability).** Under sanitizers on high-ASLR-entropy kernels,
  build-time GoogleTest discovery aborted with
  "ThreadSanitizer: unexpected memory mapping". Fixed by deferring enumeration with
  `gtest_discover_tests(... DISCOVERY_MODE PRE_TEST)` and running the sanitizer
  suite under `setarch -R` (ASLR disabled), making the sanitizer jobs reproducible
  without a build-time wrapper.

### Notes

- All GPU/Vulkan build options (`CVIS_ENABLE_CUDA`, `CVIS_ENABLE_TENSORRT`,
  `CVIS_ENABLE_VULKAN_BENCH`) default OFF; heavy serving deps (gRPC, ONNX Runtime,
  prometheus-cpp HTTP, YAML/CLI config) auto-gate OFF when absent so a bare CPU
  host yields a green build + tests.
- Deferred to later iterations: runtime model load/unload (FR-5 / OQ-6), automated
  GPU CI (R-1), the Vulkan benchmark compute path (OQ-1 / R-5), and empirical GPU
  latency baselining (NFR-1 / OQ-2).

[Unreleased]: https://github.com/vladiant/cv-inference-server/commits/main
