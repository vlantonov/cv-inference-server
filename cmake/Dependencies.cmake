# Optional third-party dependency detection.
#
# Strategy (design §5.4 adapted to a minimal CPU machine): the design's first
# choice is Conan 2 + find_package. To guarantee a green build on a bare host
# we DETECT each heavy dependency and gate the code that needs it. Anything not
# found is compiled OUT; the dependency-free core still builds and tests.
#
# Each CVIS_ENABLE_<dep> defaults to "auto": ON iff the dependency is found.
# A user can force one OFF (e.g. -DCVIS_ENABLE_GRPC=OFF) or ON (to hard-fail
# if the dep is genuinely missing).

# --- gRPC + Protobuf (cvis_api transport layer) ---
find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
    find_package(Protobuf QUIET)  # module mode fallback
endif()
find_package(gRPC CONFIG QUIET)
if(Protobuf_FOUND AND gRPC_FOUND)
    set(_cvis_grpc_default ON)
else()
    set(_cvis_grpc_default OFF)
endif()
option(CVIS_ENABLE_GRPC "Build the gRPC transport layer (needs protobuf+gRPC)" ${_cvis_grpc_default})
if(CVIS_ENABLE_GRPC AND NOT (Protobuf_FOUND AND gRPC_FOUND))
    message(FATAL_ERROR "CVIS_ENABLE_GRPC=ON but protobuf/gRPC not found via find_package")
endif()

# --- ONNX Runtime (OnnxRuntimeBackend) ---
find_package(onnxruntime CONFIG QUIET)
option(CVIS_ENABLE_ONNXRUNTIME "Build the ONNX Runtime backend (needs onnxruntime)" ${onnxruntime_FOUND})
if(CVIS_ENABLE_ONNXRUNTIME AND NOT onnxruntime_FOUND)
    message(FATAL_ERROR "CVIS_ENABLE_ONNXRUNTIME=ON but onnxruntime not found via find_package")
endif()

# --- prometheus-cpp / cpp-httplib (metrics /metrics HTTP endpoint) ---
find_package(prometheus-cpp CONFIG QUIET)
option(CVIS_ENABLE_METRICS_HTTP "Serve /metrics over HTTP (needs prometheus-cpp)" ${prometheus-cpp_FOUND})
if(CVIS_ENABLE_METRICS_HTTP AND NOT prometheus-cpp_FOUND)
    message(FATAL_ERROR "CVIS_ENABLE_METRICS_HTTP=ON but prometheus-cpp not found")
endif()

# --- yaml-cpp + CLI11 (full YAML config + CLI overrides) ---
find_package(yaml-cpp CONFIG QUIET)
find_package(CLI11 CONFIG QUIET)
if(yaml-cpp_FOUND AND CLI11_FOUND)
    set(_cvis_cfg_default ON)
else()
    set(_cvis_cfg_default OFF)
endif()
option(CVIS_ENABLE_CONFIG_YAML "Full YAML+CLI config loader (needs yaml-cpp+CLI11)" ${_cvis_cfg_default})
if(CVIS_ENABLE_CONFIG_YAML AND NOT (yaml-cpp_FOUND AND CLI11_FOUND))
    message(FATAL_ERROR "CVIS_ENABLE_CONFIG_YAML=ON but yaml-cpp/CLI11 not found")
endif()

# --- nlohmann_json (machine-readable bench output; header-only, optional) ---
find_package(nlohmann_json CONFIG QUIET)
set(CVIS_HAVE_JSON ${nlohmann_json_FOUND})
