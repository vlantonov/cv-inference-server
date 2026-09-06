# cvis_api: gRPC transport. Built ONLY when CVIS_ENABLE_GRPC is on (protobuf +
# gRPC found). Generates stubs from proto/cvis.proto at build time (design §4/§5).
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Threads REQUIRED)

set(_proto_file ${CMAKE_SOURCE_DIR}/proto/cvis.proto)
set(_gen_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${_gen_dir})

set(_pb_src  ${_gen_dir}/cvis.pb.cc)
set(_pb_hdr  ${_gen_dir}/cvis.pb.h)
set(_grpc_src ${_gen_dir}/cvis.grpc.pb.cc)
set(_grpc_hdr ${_gen_dir}/cvis.grpc.pb.h)

find_program(_protoc protoc)
find_program(_grpc_plugin grpc_cpp_plugin)

add_custom_command(
    OUTPUT ${_pb_src} ${_pb_hdr} ${_grpc_src} ${_grpc_hdr}
    COMMAND ${_protoc}
        --proto_path=${CMAKE_SOURCE_DIR}/proto
        --cpp_out=${_gen_dir}
        --grpc_out=${_gen_dir}
        --plugin=protoc-gen-grpc=${_grpc_plugin}
        ${_proto_file}
    DEPENDS ${_proto_file}
    COMMENT "Generating gRPC stubs from cvis.proto"
    VERBATIM)

add_library(cvis_api STATIC
    ${_pb_src}
    ${_grpc_src}
    src/grpc_server.cpp
)

target_include_directories(cvis_api
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${_gen_dir}
    # ModelRegistry lives in cvis_runtime; api uses its header only. Avoid a
    # link cycle (runtime -> api) -- symbols resolve at the final exe link.
    PRIVATE $<TARGET_PROPERTY:cvis_runtime,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(cvis_api PRIVATE CVIS_ENABLE_GRPC)

target_link_libraries(cvis_api
    PUBLIC cvis_core cvis_batching cvis_metrics
    PRIVATE cvis_warnings gRPC::grpc++ protobuf::libprotobuf Threads::Threads
)

add_library(cvis::api ALIAS cvis_api)
