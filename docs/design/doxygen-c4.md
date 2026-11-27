# Design Note — Doxygen Documentation Structure & C4 Component Diagram

- **Project:** `cv-inference-server`
- **Author role:** System Architect
- **Status:** Draft v0.1
- **Companions:** [architecture.md](architecture.md), [interfaces.md](interfaces.md), [README.md](../../README.md)
- **Scope:** Design-only. No Doxyfile, no CMake, no source changes. This note tells the
  C++ Developer exactly what to build.

> Grounded in the real layout: libraries under `src/` (`cvis_core`, `cvis_backends`,
> `cvis_batching`, `cvis_scheduler`, `cvis_metrics`, `cvis_api` *(gated gRPC)*,
> `cvis_runtime`) and executables under `apps/` (`cvis-server`, `cvis-bench`), with the
> **unidirectional** layer dependency rule from architecture.md §1.2.

---

## 1. Doxygen documentation structure plan

### 1.1 Toolchain prerequisites (all installed)

| Tool | Version / note | Used for |
|---|---|---|
| Doxygen | 1.15.0 | Documentation generator |
| GraphViz `dot` | installed | `HAVE_DOT = YES` — include/collaboration/call graphs |
| PlantUML | installed | Renders the `@startuml … @enduml` C4 diagram blocks |
| Java (JRE) | installed | Required by PlantUML (`plantuml.jar`) |

Doxygen settings the implementer must set to enable the above:
`HAVE_DOT = YES`, `DOT_IMAGE_FORMAT = svg`, `PLANTUML_JAR_PATH = <path to plantuml.jar>`
(or rely on the system `plantuml` wrapper via `PLANTUML_JAR_PATH`), and
`PLANTUML_INCLUDE_PATH` pointing at the directory that holds the C4-PlantUML includes
(see §2.3 fallback).

### 1.2 Output & inputs

| Doxygen key | Value | Rationale |
|---|---|---|
| `OUTPUT_DIRECTORY` | `build/docs` | Lives in the build tree; never committed. |
| HTML output | `build/docs/html` (`GENERATE_HTML = YES`, `HTML_OUTPUT = html`) | Portfolio-browsable; entry `build/docs/html/index.html`. |
| `GENERATE_LATEX` | `NO` | HTML-only portfolio target. |
| `INPUT` | `src/`, `apps/`, `docs/` (the curated `.md`/`.dox` pages) and the top-level `README.md` | Scans the real public headers + narrative pages. |
| `FILE_PATTERNS` | `*.h *.hpp *.cpp *.md *.dox` | Headers carry API docs; `.md`/`.dox` carry prose pages. |
| `RECURSIVE` | `YES` | Walk the module tree. |
| `EXTRACT_ALL` | `NO` | Only documented entities appear — keeps the API surface intentional. |
| `USE_MDFILE_AS_MAINPAGE` | the designated main page (see 1.3) | README/overview becomes the landing page. |
| `EXCLUDE` | `build/`, generated proto stubs, third-party/Conan dirs | Keep docs about *our* code, not generated gRPC stubs. |
| `GROUP_NESTED_COMPOUNDS` / `INLINE_INHERITED_MEMB` | implementer discretion | — |

> **Trade-off (OUTPUT location):** `build/docs/html` (chosen) vs. a committed `docs/html`.
> Generated HTML is a build artifact — committing it bloats the repo and causes churn.
> Keeping it under `build/` keeps the tree clean; CI/portfolio publishing regenerates it.

### 1.3 Page structure

Doxygen pages, in navigation order:

1. **Main page** (`@mainpage`) — project overview, value proposition, feature-status
   buckets (✅ CPU / ⚙️ gated / 🚧 deferred), and a link to the C4 page. Source this from a
   dedicated `docs/mainpage.dox` (or reuse `README.md` via `USE_MDFILE_AS_MAINPAGE`).
   *Recommendation:* a thin `mainpage.dox` that `@ref`s the README content, so the landing
   page stays doc-focused and the README stays Git-facing.
2. **Architecture / C4 Component page** (`@page c4_components "C4 Component View"`) — holds
   the embedded `@startuml … @enduml` block from §2, plus a short legend explaining the
   layer rule and the "gated" marker. This is the diagram the task asks for.
3. **Module group pages** — one landing section per `@defgroup` (see §3). Each cvis_*
   library's public headers are tagged `@ingroup <group>` so Doxygen auto-collates the
   module's classes/functions under its group page.
4. **(Auto) API reference** — class/file/namespace indexes Doxygen generates from the
   documented headers (`cvis::core`, `cvis::backends`, …).

### 1.4 How headers opt in

- Each public header gets a `@file` block and tags its primary declarations with
  `@ingroup <module_group>` (groups listed in §3).
- Namespaces documented once (e.g. a `namespace cvis::core { /** @brief … */ }` doc comment).
- Gated symbols (`OnnxRuntimeBackend`, `TensorRtBackend`, `MetricsHttpExposer`,
  `cvis_api` service impl) are documented unconditionally but their doc text notes the
  build gate (e.g. "@note Compiled only under `CVIS_ENABLE_TENSORRT`.") so the reference is
  complete even on a CPU-only build where the TU may be excluded.

---

## 2. C4 Component Diagram (ready to embed)

**C4 mapping.** The running **inference server process** is the C4 **Container**. The
`cvis_*` libraries and the two executables are **Components** inside it. Edges are the real
unidirectional dependencies from architecture.md §1.2 (upper layers depend on lower; `core`
depends on nothing). External actors/systems sit outside the container boundary.

### 2.1 Include strategy (recommendation)

**Recommended:** use the PlantUML **standard library** form — no vendored files, works with
the installed PlantUML:

```
!include <C4/C4_Component>
```

**Fallback** (air-gapped / pinned builds): bundle a copy of the C4-PlantUML macros under
`docs/puml/C4_Component.puml` (vendored from <https://github.com/plantuml-stdlib/C4-PlantUML>),
set Doxygen's `PLANTUML_INCLUDE_PATH = docs/puml`, and switch the include to:

```
!include C4_Component.puml
```

Only the `!include` line differs between the two; the diagram body below is identical.

### 2.2 The diagram source

Paste this verbatim into the C4 page (`@page c4_components …`) between Doxygen's
`@startuml`/`@enduml` fences.

```
@startuml cvis_c4_component
!include <C4/C4_Component>

title cv-inference-server — C4 Component View (Container: inference server process)

LAYOUT_WITH_LEGEND()

' ---------- External actors / systems ----------
Person(unary_client,     "Unary gRPC client",     "Sends Infer RPCs")
Person(stream_client,    "Streaming gRPC client", "Sends StreamInfer RPCs")
System_Ext(prometheus,   "Prometheus",            "Scrapes GET /metrics")
System_Ext(ort_runtime,  "ONNX Runtime",          "CPU EP always; CUDA EP when present [gated]")
System_Ext(trt_runtime,  "TensorRT + CUDA",       "ONNX->TRT engine execution [gated]")

Container_Boundary(server, "cv-inference-server process") {

    ' ---------- Executables (composition roots) ----------
    Component(app_server, "cvis-server",   "C++20 executable", "Thin main(); wires cvis_runtime")
    Component(app_bench,  "cvis-bench",    "C++20 executable", "Benchmark harness; drives backends directly (no api)")

    ' ---------- Libraries, top (transport/composition) to bottom (core) ----------
    Component(runtime,   "cvis_runtime",   "static lib", "Composition root: Config, ModelRegistry, ServerApp lifecycle")
    Component(api,       "cvis_api",       "static lib [gated: CVIS_ENABLE_GRPC]", "gRPC InferenceServiceImpl (unary + streaming), health/readiness; proto <-> core")
    Component(batching,  "cvis_batching",  "static lib", "DynamicBatcher: coalesce by model, flush on size OR deadline, demux, bounded-queue backpressure")
    Component(scheduler, "cvis_scheduler", "static lib", "Scheduler: round-robin fair dispatch, worker pool, bounded in-flight")
    Component(backends,  "cvis_backends",  "static lib", "IInferenceBackend + FakeBackend (always); OnnxRuntimeBackend / TensorRtBackend [gated]")
    Component(metrics,   "cvis_metrics",   "static lib", "MetricsRegistry (counters/histograms/gauges); HTTP /metrics exposer [gated: CVIS_ENABLE_METRICS_HTTP]")
    Component(core,      "cvis_core",      "static lib", "Value types: Tensor, TensorView, DataType, Shape, InferRequest/Response, Status/Expected. Depends on nothing project-internal")

    ' ---------- Internal dependency edges (unidirectional, arch §1.2) ----------
    Rel(app_server, runtime,  "wires & runs")
    Rel(app_bench,  backends, "drives directly")
    Rel(app_bench,  core,     "uses value types")
    Rel(app_bench,  metrics,  "records")

    Rel(runtime, api,       "owns gRPC server [gated]")
    Rel(runtime, batching,  "owns")
    Rel(runtime, scheduler, "owns")
    Rel(runtime, backends,  "owns via ModelRegistry")
    Rel(runtime, metrics,   "owns")
    Rel(runtime, core,      "uses")

    Rel(api,       batching,  "submit(InferRequest) -> future")
    Rel(api,       metrics,   "records")
    Rel(api,       core,      "marshals proto <-> value types")

    Rel(batching,  scheduler, "enqueue(BatchJob)")
    Rel(batching,  metrics,   "records")
    Rel(batching,  core,      "uses")

    Rel(scheduler, backends,  "executeBatch(...)")
    Rel(scheduler, metrics,   "records")
    Rel(scheduler, core,      "uses")

    Rel(backends,  core,      "uses")
    Rel(metrics,   core,      "uses")
}

' ---------- External relationships ----------
Rel(unary_client,  api,        "Infer RPC",        "gRPC/HTTP2")
Rel(stream_client, api,        "StreamInfer RPC",  "gRPC/HTTP2")
Rel(prometheus,    metrics,    "GET /metrics",     "HTTP [gated]")
Rel(backends,      ort_runtime,"inference calls [gated: CVIS_ENABLE_ONNXRUNTIME]")
Rel(backends,      trt_runtime,"inference calls [gated: CVIS_ENABLE_TENSORRT]")

@enduml
```

### 2.3 Notes for the implementer

- **"gated" marker.** C4-PlantUML has no first-class "optional" styling, so gated components
  carry the gate inline in their technology/description string (e.g. `[gated: CVIS_ENABLE_GRPC]`)
  and gated edges say `[gated]`. This keeps the diagram truthful about the CPU-only default
  without inventing non-standard shapes.
- **Fidelity to the code.** Every `Rel(...)` above is a real edge from architecture.md §1.2:
  `api → {core, batching, metrics}`, `batching → {core, scheduler, metrics}`,
  `scheduler → {core, backends, metrics}`, `backends → core`, `metrics → core`,
  `runtime → everything`, `server → runtime`, `bench → {core, backends, metrics}` (note:
  **bench does not depend on api**). Do not add edges that violate the unidirectional rule.
- **Layout.** `LAYOUT_WITH_LEGEND()` renders the C4 legend; declaring components
  top-to-bottom (runtime → core) nudges `dot` toward the layered look. Add
  `LAYOUT_TOP_DOWN()` if the auto-layout comes out too wide.

---

## 3. `@defgroup` names (one per cvis_* module)

The implementer tags each library's public headers with `@ingroup <name>`; define each group
once (e.g. in that module's primary header or a `groups.dox`).

| `@defgroup` name | Title | Maps to |
|---|---|---|
| `cvis_core`      | Core Value Types                | `src/core/`     → `cvis_core` |
| `cvis_backends`  | Inference Backends              | `src/backends/` → `cvis_backends` |
| `cvis_batching`  | Dynamic Batching               | `src/batching/` → `cvis_batching` |
| `cvis_scheduler` | Fair Scheduler                 | `src/scheduler/`→ `cvis_scheduler` |
| `cvis_metrics`   | Metrics & Observability        | `src/metrics/`  → `cvis_metrics` |
| `cvis_api`       | gRPC API (gated)               | `src/api/`      → `cvis_api` |
| `cvis_runtime`   | Runtime & Composition Root     | `src/runtime/`  → `cvis_runtime` |

Optional (recommended for the executables, so `apps/` is browsable too):

| `@defgroup` name | Title | Maps to |
|---|---|---|
| `cvis_server`    | Server Executable    | `apps/server/` → `cvis-server` |
| `cvis_bench`     | Benchmark Harness    | `apps/bench/`  → `cvis-bench` |

Group naming matches the CMake library target names 1:1 so a reader jumps between the docs,
the source tree, and the build targets without a translation table.

Example tag (illustrative, not to be committed here):
`/** @defgroup cvis_core Core Value Types */` then `/** @ingroup cvis_core */` on each type.

---

## 4. Handoff

Design is ready for the **C++ Developer** agent to implement: create the Doxyfile with the
inputs/outputs in §1, author the `@mainpage`, the `@page c4_components` page embedding the
§2 `@startuml` block, and the `@defgroup` declarations in §3, then tag the cvis_* public
headers with `@ingroup`. Do not commit generated HTML (`build/docs/html`).
