cmake_minimum_required(VERSION 3.20)
project(greenhouse_cloud_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/vcpkg/scripts/buildsystems/vcpkg.cmake")
    endif()
endif()

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)

set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../proto/sensor.proto")
set(PROTO_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
file(MAKE_DIRECTORY "${PROTO_OUT_DIR}")

find_program(PROTOC protoc REQUIRED)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

execute_process(
    COMMAND "${PROTOC}" "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}/../proto"
             "--cpp_out=${PROTO_OUT_DIR}" "--grpc_out=${PROTO_OUT_DIR}"
             "--plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}"
             "${PROTO_FILE}"
    RESULT_VARIABLE protoc_result
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
if(NOT protoc_result EQUAL 0)
    message(FATAL_ERROR "protoc failed to generate sources from sensor.proto")
endif()

add_custom_target(generate_grpc ALL
    DEPENDS "${PROTO_OUT_DIR}/sensor.pb.cc" "${PROTO_OUT_DIR}/sensor.grpc.pb.cc"
)

add_executable(greenhouse_cloud_server
    src/main.cpp
    src/logger.cpp
    src/database.cpp
    src/auth.cpp
    src/rest_server.cpp
    src/ai_analyzer.cpp
    src/grpc_service_impl.cpp
)

target_sources(greenhouse_cloud_server
    PRIVATE "${PROTO_OUT_DIR}/sensor.pb.cc"
    PRIVATE "${PROTO_OUT_DIR}/sensor.grpc.pb.cc"
)

target_include_directories(greenhouse_cloud_server PRIVATE
    src
    "${PROTO_OUT_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../proto"
)

target_link_libraries(greenhouse_cloud_server PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    SQLite::SQLite3
)

if(WIN32)
    target_compile_definitions(greenhouse_cloud_server PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_dependencies(greenhouse_cloud_server generate_grpc)
