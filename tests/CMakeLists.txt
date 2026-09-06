# Unit tests (GoogleTest + GoogleMock). GoogleTest is fetched via FetchContent
# (design §5.3): the host has libgtest but no prebuilt gmock, and FetchContent
# gives a reproducible, self-contained gtest+gmock without a package manager.
include(FetchContent)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Keep GoogleTest out of `cmake --install` / CPack: it is a test-only dependency
# and must not leak its headers/libs into the release artifact.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)
find_package(Threads REQUIRED)

add_executable(cvis_tests
    test_core.cpp
    test_fake_backend.cpp
    test_backend_factory.cpp
    test_metrics.cpp
    test_scheduler.cpp
    test_dynamic_batcher.cpp
)

target_include_directories(cvis_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(cvis_tests
    PRIVATE
        cvis_core cvis_backends cvis_batching cvis_scheduler cvis_metrics
        GTest::gtest_main GTest::gmock
        cvis_warnings cvis_sanitizers Threads::Threads
)

# DISCOVERY_MODE PRE_TEST (QA defect D-1): defer test enumeration to `ctest`
# invocation instead of running the freshly-built binary at build time. Under
# sanitizers on high-ASLR-entropy kernels, the post-build discovery run aborts
# with "ThreadSanitizer: unexpected memory mapping"; deferring discovery makes
# the sanitizer CI job reproducible without a build-time setarch wrapper.
gtest_discover_tests(cvis_tests DISCOVERY_MODE PRE_TEST)
