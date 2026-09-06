# Portfolio writeup outline — cv-inference-server

> Lightweight outline for a future long-form portfolio piece (blog / Habr /
> vlantonov.github.io). Not a finished article — a skeleton to expand once GPU
> benchmark numbers exist. The README is the primary artifact; this is optional.

## Working title
"Building the CPU-testable core of a real-time CV inference server in C++20"

## Angle
The interesting engineering isn't "call the model" — it's the pipeline around it:
dynamic batching against a latency budget, fair scheduling with backpressure, and a
design that degrades from a GPU host to a CPU-only laptop without `#ifdef` sprawl in
the core.

## Outline
1. **Problem framing** — what real-time CV serving actually demands (bursty
   concurrency, latency budgets, no unbounded memory growth, measurable throughput).
2. **Layered design** — unidirectional module dependencies; why `core`/`batching`/
   `scheduler`/`backends` are independently unit-testable on CPU. (Reference the
   Mermaid diagram.)
3. **Dynamic batching** — flush on size OR deadline, correct demux by correlation
   id, the disable path, and bounded-queue backpressure. Show the algorithm.
4. **Fair scheduling** — round-robin no-starvation dispatch + bounded in-flight.
5. **Graceful degradation** — the gating strategy: auto-detect heavy deps, compile
   out what's missing, keep a green CPU build. Contrast with hard-required deps.
6. **Making it measurable** — the transport-free bench harness; batch × concurrency
   sweep; JSON/CSV + reproducibility metadata. *(Insert GPU numbers when available.)*
7. **Testing story** — 34 GoogleTest tests, GoogleMock over `IInferenceBackend`, and
   the ASan/TSan matrix. The **D-1** war story: sanitizer test discovery aborting on
   high-ASLR kernels and the `PRE_TEST` + `setarch -R` fix — a concrete, subtle bug
   worth explaining.
8. **What's deferred and why** — honest scope: GPU CI (R-1), runtime load/unload
   (FR-5/OQ-6), Vulkan (OQ-1/R-5).

## TODO before publishing
- Fill in real GPU (CUDA/TensorRT) benchmark numbers.
- Add an end-to-end gRPC client example once a deps-available build is captured.
