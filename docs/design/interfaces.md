# Design Specification — Key C++ Interfaces

- **Project:** `cv-inference-server`
- **Companion to:** [architecture.md](architecture.md)
- **Author role:** System Architect
- **Status:** Draft v0.1

> These are **header-level sketches** — signatures, ownership, and thread-safety
> contracts only. No implementations. The C++ Developer fills in bodies.
> Standard: C++20 (C++23 `std::expected` where available; otherwise a vendored
> `tl::expected` aliased as `cvis::Expected`). No raw owning pointers anywhere.

---

## 1. `cvis::core` — value types

```cpp
namespace cvis::core {

enum class DataType { kUnspecified, kFloat32, kFloat16, kInt8, kUInt8, kInt32, kInt64 };

// Number of bytes for one element of a dtype.
std::size_t byteSize(DataType dt) noexcept;

using Shape = std::vector<std::int64_t>;   // dimensions; -1 permitted for dynamic

// Owning, move-only tensor. RAII over a contiguous byte buffer (NFR-5).
class Tensor {
public:
    Tensor() = default;
    Tensor(std::string name, DataType dtype, Shape shape);   // allocates buffer
    Tensor(Tensor&&) noexcept = default;
    Tensor& operator=(Tensor&&) noexcept = default;
    Tensor(const Tensor&) = delete;                          // move-only: no accidental copies
    Tensor& operator=(const Tensor&) = delete;
    Tensor clone() const;                                    // explicit deep copy when needed

    const std::string& name() const noexcept;
    DataType dtype() const noexcept;
    const Shape& shape() const noexcept;
    std::size_t byteCount() const noexcept;

    std::span<std::byte> bytes() noexcept;                   // mutable view into owned buffer
    std::span<const std::byte> bytes() const noexcept;

private:
    std::string name_;
    DataType dtype_ = DataType::kUnspecified;
    Shape shape_;
    std::vector<std::byte> data_;                            // owns the payload
};

// Non-owning, read-only view over an external buffer (e.g. a gRPC arena buffer).
// Ownership: DOES NOT own. Valid only while the source outlives it.
struct TensorView {
    std::string_view name;
    DataType dtype = DataType::kUnspecified;
    std::span<const std::int64_t> shape;
    std::span<const std::byte> data;
};

// Canonical status/error carried across module boundaries (maps to grpc::Status).
enum class StatusCode { kOk, kInvalidArgument, kNotFound, kFailedPrecondition,
                        kResourceExhausted, kUnavailable, kInternal, kUnimplemented };

struct Status {
    StatusCode code = StatusCode::kOk;
    std::string message;
    bool ok() const noexcept { return code == StatusCode::kOk; }
    static Status Ok() { return {}; }
};

// Project-wide result alias. Prefer over exceptions across boundaries (see arch §9).
template <class T>
using Expected = std::expected<T, Status>;   // or tl::expected fallback

// One logical inference unit flowing through the pipeline.
struct InferRequest {
    std::string model_id;
    std::string correlation_id;               // echoed on response (FR-12/FR-14)
    std::vector<Tensor> inputs;               // owns its input tensors
};

struct InferResponse {
    std::string model_id;
    std::string correlation_id;
    std::vector<Tensor> outputs;
    Status status;                            // per-item status (streaming, FR-13/FR-14)
};

} // namespace cvis::core
```

**Ownership / thread-safety:** `Tensor` owns its buffer and is move-only.
`TensorView` is a borrow — never stored past the source's lifetime. `InferRequest`
/`InferResponse` are plain owning value types, moved (not copied) through the
pipeline. These types are **not** internally synchronized; thread-safety is provided
by the containers/queues that hold them.

---

## 2. `cvis::backends` — inference backend abstraction

```cpp
namespace cvis::backends {

struct ModelConfig {
    std::string model_id;
    std::string path;                         // filesystem path to ONNX (OQ-3)
    std::string backend;                      // "onnxruntime" | "tensorrt"
    bool use_gpu = false;                     // request CUDA EP (FR-22 fallback if false/unavailable)
    // backend-specific opaque options (e.g. TRT engine cache dir) live here
    std::map<std::string, std::string> options;
};

struct ModelMetadata {
    std::string model_id;
    std::string backend;
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
    bool ready = false;
};

// Pure abstraction. A backend instance owns one loaded model (one session/engine).
// Adding a backend never touches api/scheduler (FR-8, NFR-12).
class IInferenceBackend {
public:
    virtual ~IInferenceBackend() = default;

    // Load/prepare the model. Called once at startup (OQ-6). Fallible, no throw.
    virtual core::Status load(const ModelConfig& cfg) = 0;

    virtual const ModelMetadata& metadata() const noexcept = 0;

    // Execute a coalesced batch. Requests are borrowed (non-owning span of
    // pointers) and outlive the call. Returns one response per request,
    // index-aligned (demux contract, FR-18). Thread-safety declared by impl;
    // default contract: the scheduler guarantees a single concurrent call per
    // backend instance (see arch §6/§9).
    virtual core::Expected<std::vector<core::InferResponse>>
    executeBatch(std::span<const core::InferRequest* const> batch) = 0;
};

// Factory: name -> backend instance. Registered impls are compile-time gated
// (OnnxRuntime always; TensorRt under CVIS_ENABLE_TENSORRT; Fake in tests).
class BackendFactory {
public:
    using Creator = std::function<std::unique_ptr<IInferenceBackend>()>;
    void registerBackend(std::string name, Creator creator);       // startup only
    // Returns UNIMPLEMENTED-style error if backend not built/registered (FR-9).
    core::Expected<std::unique_ptr<IInferenceBackend>> create(std::string_view name) const;
};

} // namespace cvis::backends
```

**Concrete impls (headers only here):**
- `OnnxRuntimeBackend : IInferenceBackend` — owns an `Ort::Session`; CPU EP always,
  CUDA EP appended when `use_gpu && CVIS_ENABLE_CUDA` and a device is present, else
  transparent CPU fallback (FR-22). Always compiled.
- `TensorRtBackend : IInferenceBackend` — ONNX→TRT build + on-disk engine cache
  keyed by model hash + TRT version + device (OQ-3, R-4). Compiled only under
  `CVIS_ENABLE_TENSORRT`.
- `FakeBackend : IInferenceBackend` — deterministic echo/identity; no external deps;
  used to unit-test batching/scheduler on CPU CI (FR-8, NFR-10). Also a GoogleMock
  `MockBackend` for expectation-based tests.

**Ownership:** backend instances are held as `std::unique_ptr` by the
`ModelRegistry`. **Thread-safety:** an instance is used by at most one scheduler
worker at a time by contract (arch §9); impls need not be internally reentrant
unless they opt in.

---

## 3. `cvis::batching` — dynamic batcher

```cpp
namespace cvis::batching {

struct BatchingConfig {
    bool enabled = true;                      // false == max_batch_size 1 (FR-19)
    std::size_t max_batch_size = 8;           // size trigger (FR-16)
    std::chrono::milliseconds max_wait{5};    // latency budget / deadline (FR-16/NFR-3)
    std::size_t max_queue_depth = 256;        // bounded queue -> backpressure (FR-23/NFR-6)
};

// Thread-safe front door for submitting requests. Coalesces per model_id and
// flushes to the Scheduler on size OR deadline (arch §7).
class DynamicBatcher {
public:
    DynamicBatcher(BatchingConfig cfg,
                   scheduler::Scheduler& scheduler,   // borrowed, outlives batcher
                   metrics::MetricsRegistry& metrics);
    ~DynamicBatcher();                                // joins flush threads (RAII)

    DynamicBatcher(const DynamicBatcher&) = delete;
    DynamicBatcher& operator=(const DynamicBatcher&) = delete;

    // THREAD-SAFE. Returns a future fulfilled when this request's slice of the
    // batch result is ready, or RESOURCE_EXHAUSTED immediately if the model's
    // queue is full (FR-23).
    core::Expected<std::future<core::InferResponse>>
    submit(core::InferRequest request);

    void shutdown();                                  // drain + stop flush threads
};

} // namespace cvis::batching
```

**Ownership:** borrows `Scheduler` and `MetricsRegistry` by reference (composition
root guarantees lifetime). Owns per-model queues + flush thread(s).
**Thread-safety:** `submit` is safe for concurrent callers (gRPC handler threads).
Internal queues guarded by mutex + `condition_variable`; deadline enforced by a
timed wait (NFR-3).

---

## 4. `cvis::scheduler` — fair dispatch + backpressure

```cpp
namespace cvis::scheduler {

struct SchedulerConfig {
    std::size_t workers = 1;                   // CPU-bound ORT: default 1 (arch §6)
    std::size_t max_in_flight = 4;             // bounded in-flight batches (FR-23)
};

// One coalesced unit of work handed from batcher to scheduler.
struct BatchJob {
    std::string model_id;
    std::vector<core::InferRequest> requests;                       // owned, moved in
    std::vector<std::promise<core::InferResponse>> promises;        // index-aligned demux
};

// Owns worker threads. Fair (round-robin) pop across per-model queues so no model
// starves (FR-21). Bounds in-flight batches (FR-23).
class Scheduler {
public:
    Scheduler(SchedulerConfig cfg, metrics::MetricsRegistry& metrics);
    ~Scheduler();                              // joins workers (RAII)

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    // Bind a model_id to the backend instance that will execute its batches.
    void registerModel(std::string model_id, backends::IInferenceBackend& backend);

    // THREAD-SAFE. Enqueue a batch for dispatch. Blocks or returns
    // RESOURCE_EXHAUSTED when max_in_flight for the model is reached (policy).
    core::Status enqueue(BatchJob job);

    void shutdown();                           // drain + stop workers
};

} // namespace cvis::scheduler
```

**Ownership:** borrows `MetricsRegistry` and (per model) `IInferenceBackend` by
reference; owns worker threads and internal queues. Fulfils each `promise` after
`executeBatch`, demuxing outputs by index (FR-18).
**Thread-safety:** `enqueue` and `registerModel` are safe; worker loop pops fairly
under a mutex; `max_in_flight` tracked with an atomic/guarded counter.

---

## 5. `cvis::metrics` — Prometheus registry

```cpp
namespace cvis::metrics {

// Thread-safe façade over prometheus-cpp. Injected by reference everywhere so no
// module hard-couples to prometheus-cpp (arch §2).
class MetricsRegistry {
public:
    MetricsRegistry();

    void incRequests(std::string_view model_id) noexcept;          // FR-26
    void incErrors(std::string_view model_id) noexcept;
    void observeLatency(std::string_view model_id,
                        std::chrono::nanoseconds d) noexcept;
    void observeBatchSize(std::string_view model_id, std::size_t n) noexcept;
    void setQueueDepth(std::string_view model_id, std::size_t n) noexcept;

    // Renders Prometheus text exposition format (OQ-4). Served at GET /metrics.
    std::string scrape() const;
};

// Owns the HTTP listener thread; serves /metrics. RAII stop on destruction.
class MetricsHttpExposer {
public:
    MetricsHttpExposer(MetricsRegistry& registry, std::string bind_addr, std::uint16_t port);
    ~MetricsHttpExposer();
};

} // namespace cvis::metrics
```

**Thread-safety:** fully thread-safe (all record methods callable from any thread).

---

## 6. `cvis::runtime` — config & lifecycle (composition root)

```cpp
namespace cvis::runtime {

struct Config {
    std::string grpc_bind = "0.0.0.0:50051";
    std::string metrics_bind = "0.0.0.0:9090";
    std::vector<backends::ModelConfig> models;      // startup models (OQ-6)
    batching::BatchingConfig batching;
    scheduler::SchedulerConfig scheduler;
};

// YAML file + CLI flag overrides (OQ-5). CLI wins over file.
core::Expected<Config> loadConfig(int argc, char** argv);

// Holds loaded backends; answers readiness (FR-3/FR-25). shared_mutex guarded to
// allow future runtime load/unload (OQ-6 deferred).
class ModelRegistry {
public:
    core::Status loadFromConfig(const Config& cfg, backends::BackendFactory& factory);
    std::vector<backends::ModelMetadata> list() const;             // FR-3
    bool anyReady() const noexcept;                                // FR-25 readiness
    backends::IInferenceBackend* find(std::string_view model_id) noexcept; // non-owning
};

// Wires everything; owns the gRPC server, batcher, scheduler, registry, metrics.
// Startup errors -> clear diagnostic + non-zero exit (NFR-13).
class ServerApp {
public:
    static core::Expected<ServerApp> create(Config cfg);
    core::Status run();                        // blocks until shutdown signal
    void requestShutdown() noexcept;           // graceful drain
};

} // namespace cvis::runtime
```

**Ownership:** `ServerApp` is the single owner of all long-lived objects
(`ModelRegistry` → backends, `Scheduler`, `DynamicBatcher`, `MetricsRegistry`,
gRPC server). Everything below borrows by reference, so lifetimes are unambiguous
and RAII teardown happens in reverse construction order (NFR-5).

---

## 7. `cvis::api` — gRPC service glue (no new abstractions)

The generated `cvis::v1::InferenceService::Service` is subclassed by
`InferenceServiceImpl`, which:
- marshals `cvis::v1::InferRequest` (proto) ↔ `core::InferRequest` (moving bytes,
  borrowing via `TensorView` where possible),
- calls `DynamicBatcher::submit`,
- awaits the returned future and writes the `InferResponse` (unary) or streams
  responses preserving `correlation_id` (streaming, FR-14),
- maps `core::Status` → `grpc::Status` for structured errors (FR-13).

It holds only **references** to `DynamicBatcher`, `ModelRegistry`, and
`MetricsRegistry`. It is stateless per-call and safe under the gRPC thread pool.

---

*Interfaces are ready for the C++ Developer agent to implement — start with Phase 1
(`core` + `OnnxRuntimeBackend` CPU + `FakeBackend` + unary `Infer`).*
