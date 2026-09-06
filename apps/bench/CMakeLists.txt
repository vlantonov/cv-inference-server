# cvis_bench: benchmark harness (design §8). Links only core/backends/metrics
# (never api). The CPU path always runs; cuda/vulkan cells report "unavailable"
# unless their build flags are enabled (FR-31).
find_package(Threads REQUIRED)

add_executable(cvis_bench main.cpp)

target_link_libraries(cvis_bench
    PRIVATE cvis_core cvis_backends cvis_metrics cvis_warnings cvis_sanitizers Threads::Threads
)

if(CVIS_ENABLE_CUDA)
    target_compile_definitions(cvis_bench PRIVATE CVIS_ENABLE_CUDA)
endif()

if(CVIS_ENABLE_VULKAN_BENCH)
    # Best-effort Vulkan compute path (design R-5). Only compiled when requested;
    # the harness still reports the cell as unavailable until a real path lands.
    find_package(Vulkan QUIET)
    target_compile_definitions(cvis_bench PRIVATE CVIS_ENABLE_VULKAN_BENCH)
    if(Vulkan_FOUND)
        target_link_libraries(cvis_bench PRIVATE Vulkan::Vulkan)
    endif()
endif()

if(CVIS_HAVE_JSON)
    target_compile_definitions(cvis_bench PRIVATE CVIS_HAVE_JSON)
    target_link_libraries(cvis_bench PRIVATE nlohmann_json::nlohmann_json)
endif()

set_target_properties(cvis_bench PROPERTIES OUTPUT_NAME cvis-bench)
