# Software Requirements Specification — CV Inference Server

- **Project:** `cv-inference-server`
- **Document type:** Software Requirements Specification (SRS)
- **Status:** Draft v0.1 (initial groundwork)
- **Author role:** Requirements Analyst
- **Date:** 2026-09-07

---

## 1. Introduction & Scope

### 1.1 Purpose
This document specifies **what** a real-time computer-vision (CV) model serving
system must do. It is the authoritative requirements baseline for the later
design, implementation, and test stages. It intentionally avoids prescribing
architecture, internal APIs, or library selection — those are decided by the
System Architect and Developer.

### 1.2 Portfolio context
`cv-inference-server` is a single-maintainer **portfolio project**. Its goals are
to demonstrate competence in:
- Modern C++ (C++17/20) systems programming and RAII resource management.
- Pluggable backend abstraction over multiple inference runtimes.
- Low-latency network service design with gRPC (unary + streaming).
- Throughput optimization via dynamic request batching and compute scheduling.
- Reproducible performance benchmarking across compute backends.

Requirements are scoped to be demonstrable by one maintainer; enterprise-scale
concerns (multi-tenancy, auth/RBAC, horizontal autoscaling, HA clustering) are
explicitly out of scope unless later promoted.

### 1.3 In scope
- Loading and serving pre-trained CV models via pluggable inference backends
  (ONNX Runtime and TensorRT).
- gRPC API offering unary and streaming inference.
- Runtime dynamic batching honoring a configurable latency budget.
- Scheduling of batched work onto available compute (CPU / GPU).
- Health and metrics endpoints for observability.
- A benchmark harness comparing CPU / CUDA / Vulkan compute paths (see
  Open Question OQ-1 regarding Vulkan feasibility).

### 1.4 Out of scope (this iteration)
- Model **training**, fine-tuning, or conversion pipelines (models are supplied
  pre-built in a backend-supported format).
- Authentication, authorization, TLS/mTLS, rate limiting, and multi-tenant
  isolation.
- Horizontal scaling, service discovery, load balancing across nodes, and
  orchestration (Kubernetes, etc.).
- Web UI / dashboard front-end (metrics are exposed for an external scraper).
- Persistent storage of inference results or a database layer.
- Non-CV model modalities (NLP, audio) beyond incidental support.

---

## 2. Stakeholders & Personas

| ID | Persona | Description | Primary needs |
|----|---------|-------------|---------------|
| P-1 | **ML Engineer (Deployer)** | Deploys a pre-trained CV model to the server and configures its backend. | Simple model registration, backend selection, clear load/error feedback. |
| P-2 | **Client Application (Frame Producer)** | A program streaming image frames and consuming predictions in real time. | Low-latency streaming API, stable contract, backpressure handling. |
| P-3 | **Performance Engineer** | Runs benchmarks to compare compute backends and tune batching. | Reproducible benchmark harness, latency/throughput reports. |
| P-4 | **Operator / SRE (lightweight)** | Monitors a running instance. | Health checks and metrics for liveness/readiness and throughput. |
| P-5 | **Maintainer (Portfolio owner)** | Builds, tests, and evolves the project. | Clean CMake build, tests, sanitizers, CPU-only fallback for CI. |

---

## 3. Functional Requirements (FR)

Each requirement is independently testable. "Shall" denotes a mandatory
requirement; "should" denotes a desirable one.

### 3.1 Model loading & management
- **FR-1:** The server shall load a pre-trained model from a
  filesystem path specified at startup or via a management request.
- **FR-2:** The server shall report load success or a structured
  failure (with a diagnostic reason) when a model cannot be loaded.
- **FR-3:** The server shall expose the set of currently loaded
  models and, for each, its identifier, selected backend, and readiness state.
- **FR-4:** The server shall reject inference requests that target an
  unknown or not-ready model with a well-defined error response.
- **FR-5:** The server should support unloading a model to release
  its associated compute/memory resources without restarting the process.

### 3.2 Backend selection & abstraction
- **FR-6:** The server shall support at least two interchangeable
  inference backends: **ONNX Runtime** and **TensorRT**.
- **FR-7:** The backend used for a given model shall be selectable via
  configuration (per-model), without recompiling the server.
- **FR-8:** Backends shall be pluggable behind a common abstraction so
  that adding a new backend does not require changes to the API or scheduling
  layers (verified by the presence of the abstraction boundary and a mock/test
  backend).
- **FR-9:** If a requested backend is unavailable at runtime (e.g. no
  GPU / TensorRT not built), the server shall fail loading that model with a
  clear reason and shall not crash.

### 3.3 gRPC API — unary & streaming inference
- **FR-10:** The server shall expose a gRPC service supporting a
  **unary** inference call: one request containing input tensor(s) returns one
  response containing output tensor(s).
- **FR-11:** The server shall expose a gRPC **bidirectional streaming**
  inference call for real-time frame streams, returning predictions as they are
  produced.
- **FR-12:** The inference request/response contract shall carry, at
  minimum: target model identifier, input tensor payload(s) with shape and
  dtype metadata, and a correlation/request identifier echoed on the response.
- **FR-13:** The server shall return a structured error (status code +
  message) for malformed requests (shape/dtype mismatch, missing model, etc.).
- **FR-14:** The streaming API shall preserve correlation between each
  input frame and its corresponding output so clients can match results to
  inputs.

### 3.4 Dynamic batching
- **FR-15:** The server shall coalesce concurrently arriving inference
  requests for the same model into a single batch before execution.
- **FR-16:** Dynamic batching shall be bounded by a configurable
  **maximum batch size** and a configurable **maximum queue/wait time**
  (latency budget); a batch shall dispatch when either bound is reached.
- **FR-17:** Batching shall be transparent to clients — a batched
  request shall return the same result it would have returned unbatched.
- **FR-18:** The server shall correctly demultiplex a batched result
  back to each originating request/stream.
- **FR-19:** Dynamic batching shall be configurable to be disabled
  (batch size 1) for latency-sensitive or debugging scenarios.

### 3.5 GPU / compute scheduling
- **FR-20:** The server shall schedule batched work onto available
  compute resources (CPU and, when present, GPU).
- **FR-21:** The scheduler shall serve multiple models and multiple
  concurrent clients without starving any single model indefinitely.
- **FR-22:** When GPU resources are unavailable, the scheduler shall
  fall back to CPU execution (see NFR-8) rather than failing.
- **FR-23:** The scheduler should bound the number of in-flight batches
  to avoid unbounded memory growth under load (backpressure).

### 3.6 Health & metrics
- **FR-24:** The server shall expose a **liveness** check indicating the
  process is running.
- **FR-25:** The server shall expose a **readiness** check indicating
  whether at least one model is loaded and able to serve.
- **FR-26:** The server shall expose runtime **metrics** including at
  minimum: request count, error count, per-request latency, effective batch
  size, and queue depth.
- **FR-27:** Metrics shall be retrievable by an external consumer via a
  documented, scrape-friendly mechanism (format decided at design time).

### 3.7 Benchmark harness
- **FR-28:** The project shall provide a benchmark harness that measures
  inference **latency** (p50/p99) and **throughput** for a given model and
  backend.
- **FR-29:** The harness shall run the same workload across the
  available compute paths — **CPU**, **CUDA**, and **Vulkan** — and emit a
  comparative report (subject to OQ-1 on Vulkan availability).
- **FR-30:** The harness shall vary key parameters (batch size,
  concurrency) and record results in a machine-readable form (e.g. CSV/JSON) for
  reproducibility.
- **FR-31:** The harness shall be runnable in a CPU-only environment so
  that at least the CPU path produces results without GPU hardware.

---

## 4. Non-Functional Requirements (NFR)

Targets are portfolio-appropriate and may be refined once a reference model and
hardware baseline are fixed (see OQ-2).

- **NFR-1 (Latency):** For a nominal reference model on the GPU path,
  single-request unary inference **p50 latency should be ≤ 50 ms** and
  **p99 ≤ 150 ms** under light load. Exact numbers to be baselined (OQ-2).
- **NFR-2 (Throughput):** Dynamic batching shall measurably increase
  GPU throughput (inferences/sec) versus batch-size-1 for the same model, and
  the benchmark harness shall demonstrate this improvement.
- **NFR-3 (Latency budget adherence):** With a configured max wait time
  `W`, no request shall be held in the batching queue longer than `W` plus
  scheduling overhead before dispatch.
- **NFR-4 (Concurrency):** The server shall handle multiple concurrent
  gRPC clients and streams without data races or corruption, verified under a
  thread/…-sanitizer build.
- **NFR-5 (Resource safety):** All compute and device resources shall be
  managed via RAII; there shall be no leaks reported by the leak/address
  sanitizer on the standard test suite.
- **NFR-6 (Memory bounds):** Under sustained load the server's queued
  work shall be bounded (per FR-23) so memory usage does not grow without limit.
- **NFR-7 (Portability — build):** The project shall build on **Linux**
  with **CMake** using a modern C++ compiler supporting **C++17/20**.
- **NFR-8 (Portability — CPU-only fallback):** The server and its test
  suite shall build and run in a **CPU-only** environment (no GPU, no
  CUDA/TensorRT), exercising at least the ONNX Runtime CPU path.
- **NFR-9 (Observability):** Health and metrics (FR-24–FR-27) shall be
  available whenever the server is running, with negligible steady-state
  overhead.
- **NFR-10 (Testability):** Core logic (batching, scheduling, backend
  abstraction) shall be covered by automated unit tests using the project's
  test framework (GoogleTest or Catch2), runnable via CTest.
- **NFR-11 (Reproducibility):** Benchmark runs shall record their
  configuration (model, backend, batch/concurrency, hardware) alongside results
  so a run can be reproduced.
- **NFR-12 (Maintainability):** Adding a new inference backend or a new
  compute path shall not require modifying the gRPC API contract.
- **NFR-13 (Startup robustness):** Configuration or model-load errors at
  startup shall produce a clear diagnostic and a non-zero exit (or a
  not-ready state) rather than a crash.

---

## 5. Constraints & Assumptions

### 5.1 Constraints
- **C-1:** Implementation language is **C++ (C++17/20)**.
- **C-2:** Build system is **CMake**; tests run via **CTest**.
- **C-3:** Primary target OS is **Linux** (x86-64).
- **C-4:** Named inference backends are **ONNX Runtime** and **TensorRT**.
- **C-5:** Transport/API is **gRPC** (Protocol Buffers contract).
- **C-6:** Test framework is **GoogleTest or Catch2** (final choice at design).
- **C-7:** The build shall support **address**, **leak**, and **thread**
  sanitizer configurations for verification.

### 5.2 Assumptions
- **A-1:** Models are supplied pre-trained in a format the selected backend
  can load (e.g. ONNX for ONNX Runtime; a TensorRT engine/ONNX for TensorRT).
- **A-2:** GPU hardware (NVIDIA/CUDA) is **optional** at runtime; CI and the
  default developer path may be CPU-only.
- **A-3:** Clients are trusted and on a trusted network (no auth in scope).
- **A-4:** A single server process serves one host's compute; no clustering.
- **A-5:** Input data are image-like tensors typical of CV workloads.

---

## 6. Acceptance Criteria

A capability is "done" when the following are demonstrable and tested:

- **AC-1 (Model management):** A pre-trained model can be loaded, listed as
  ready, served, and unloaded; loading an invalid model yields a clear error
  and no crash. (FR-1–FR-5)
- **AC-2 (Backends):** The same logical model can be served via the ONNX
  Runtime backend and, where hardware permits, the TensorRT backend, selected by
  configuration; an unavailable backend fails gracefully. (FR-6–FR-9)
- **AC-3 (gRPC API):** A client can perform a unary inference and a
  bidirectional streaming inference and correctly match each output to its
  input; malformed requests return structured errors. (FR-10–FR-14)
- **AC-4 (Dynamic batching):** Under concurrent load, requests are batched
  within the configured size and latency budget, results are correctly
  demultiplexed, and disabling batching yields batch size 1. (FR-15–FR-19)
- **AC-5 (Scheduling):** Multiple models/clients are served concurrently
  without starvation; with no GPU present the system falls back to CPU.
  (FR-20–FR-23)
- **AC-6 (Health & metrics):** Liveness/readiness reflect real state and
  metrics expose request/error counts, latency, batch size, and queue depth.
  (FR-24–FR-27)
- **AC-7 (Benchmarks):** The harness produces a machine-readable comparative
  report of latency (p50/p99) and throughput across the available compute paths,
  and runs at least the CPU path with no GPU present. (FR-28–FR-31)
- **AC-8 (Quality gates):** The project builds on Linux with CMake, unit
  tests pass under CTest, and sanitizer builds report no leaks or data races on
  the test suite. (NFR-4, NFR-5, NFR-7, NFR-8, NFR-10)

---

## 7. Open Questions / Risks

- **OQ-1 — Vulkan vs. named backends (must resolve before design):** The
  benchmark requirement (FR-29) names a **Vulkan** compute path, but the named
  inference backends are **ONNX Runtime** and **TensorRT**. TensorRT is
  CUDA-only, and ONNX Runtime's Vulkan support is limited/experimental. There is
  a tension: *how is the Vulkan path produced?* Options to decide:
  (a) drop Vulkan and benchmark CPU vs. CUDA only;
  (b) use ONNX Runtime execution providers that map to Vulkan-capable paths
  where available;
  (c) add a separate Vulkan-based compute backend purely for the benchmark
  (expands FR-6 backend set). **Stakeholder/Architect decision required.**
- **OQ-2 — Performance baseline:** NFR-1/NFR-2 targets need a concrete
  reference model and hardware baseline to become measurable. Which model
  (e.g. a standard CV classifier/detector) and which GPU/CPU baseline?
- **OQ-3 — Model source & formats:** Which exact input formats are supported
  per backend (ONNX only? prebuilt TensorRT engines? on-the-fly ONNX→TRT
  conversion)? Affects FR-1, FR-6, A-1.
- **OQ-4 — Metrics format:** Should metrics be Prometheus-exposition,
  OpenTelemetry, or a simple JSON endpoint? (FR-27, decided at design.)
- **OQ-5 — Config mechanism:** Is model/backend configuration file-based,
  flag-based, or via a management gRPC method? (FR-1, FR-7)
- **OQ-6 — Dynamic model management scope:** Is runtime load/unload
  (FR-5, FR-1 management request) required for v1, or is startup-only loading
  sufficient for the portfolio milestone?
- **R-1 (Risk) — GPU CI:** Sanitizer/CI environments are likely CPU-only, so
  GPU/CUDA/TensorRT paths may be validated only manually on GPU hardware. This
  constrains automated verification of GPU-specific FRs/NFRs.
- **R-2 (Risk) — Scope creep:** Backend abstraction + batching + scheduling +
  multi-path benchmarks is broad for a single maintainer; prioritization
  (e.g. ONNX Runtime CPU path first) may be needed.

---

*Requirements are ready for the System Architect agent to design against.*
