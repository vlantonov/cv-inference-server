# cvis_server: thin main() wiring cvis_runtime (design §2). Builds on a bare CPU
# machine; serves gRPC only when CVIS_ENABLE_GRPC was enabled at configure time.
add_executable(cvis_server main.cpp)

target_link_libraries(cvis_server
    PRIVATE cvis_runtime cvis_warnings cvis_sanitizers
)

set_target_properties(cvis_server PROPERTIES OUTPUT_NAME cvis-server)

# Stage the server binary for `cmake --install` / CPack (design §5.1).
install(TARGETS cvis_server RUNTIME DESTINATION bin COMPONENT applications)
