# Sanitizer flags exposed as an INTERFACE target (design §5.2, NFR-4/NFR-5).
# ASan and TSan are mutually exclusive; select via CVIS_SANITIZER.
add_library(cvis_sanitizers INTERFACE)

if(CVIS_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(CVIS_SANITIZER STREQUAL "thread")
            message(STATUS "Sanitizer: ThreadSanitizer (TSan)")
            target_compile_options(cvis_sanitizers INTERFACE -fsanitize=thread -g -O1 -fno-omit-frame-pointer)
            target_link_options(cvis_sanitizers INTERFACE -fsanitize=thread)
        else()
            message(STATUS "Sanitizer: AddressSanitizer + LeakSanitizer (ASan/LSan)")
            target_compile_options(cvis_sanitizers INTERFACE -fsanitize=address,undefined -g -O1 -fno-omit-frame-pointer)
            target_link_options(cvis_sanitizers INTERFACE -fsanitize=address,undefined)
        endif()
    else()
        message(WARNING "CVIS_ENABLE_SANITIZERS ignored: unsupported compiler")
    endif()
endif()
