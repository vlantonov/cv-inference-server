# Shared warning flags exposed as an INTERFACE target so every module opts in
# the same way (self-review discipline: warnings-as-signal).
add_library(cvis_warnings INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cvis_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wnon-virtual-dtor -Woverloaded-virtual)
elseif(MSVC)
    target_compile_options(cvis_warnings INTERFACE /W4 /permissive-)
endif()
