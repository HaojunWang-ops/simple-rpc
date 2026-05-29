cmake_minimum_required(VERSION 3.16)

project(simple_rpc_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
    rpc_header.proto
    user.proto
)

add_library(proto_objs 
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_include_directories(proto_objs PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(proto_objs PUBLIC
    protobuf::libprotobuf
)

add_library(rpc_core
    src/rpc_channel.cc
    src/rpc_provider.cc
)

target_include_directories(rpc_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(rpc_core PUBLIC
    proto_objs
)

add_executable(client src/client.cc)

target_link_libraries(client PRIVATE rpc_core)

add_executable(server src/server.cc)

target_link_libraries(server PRIVATE rpc_core)