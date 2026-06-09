cmake_minimum_required(VERSION 3.25)

project(ClientExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Homebrew
include_directories(
    /opt/homebrew/include
    ../../protocol/proto_res
)

link_directories(
    /opt/homebrew/lib
)

file(GLOB PROTO_SRCS
    ../../protocol/proto_res/*.pb.cc
)

add_executable(client_ssl
    client_ssl.cpp
    ${PROTO_SRCS}
)

add_executable(client
    client.cpp
    ${PROTO_SRCS}
)

add_executable(client_nb_connect client_nb_connect.cpp)

find_package(Threads REQUIRED)

# 设置 Abseil 的查找路径
set(absl_DIR /opt/homebrew/lib/cmake/absl)

find_package(absl REQUIRED)
if (absl_FOUND)
    message(STATUS "absl found at ${absl_INCLUDE_DIRS}")
    include_directories(${absl_INCLUDE_DIRS})
endif()

find_package(OpenSSL 1.1 REQUIRED)
if (OpenSSL_FOUND)
    message(STATUS "OpenSSL found at ${OpenSSL_INCLUDE_DIRS}")
    include_directories(${OpenSSL_INCLUDE_DIRS})
endif()

target_link_libraries(client_ssl
    PRIVATE
    absl::log
    absl::check
    absl::base
    absl::strings
    absl::raw_logging_internal
    absl::log_internal_message
    absl::spinlock_wait
    protobuf
    Threads::Threads
    OpenSSL::SSL  # 使用 OpenSSL 的模块
    OpenSSL::Crypto  # 使用 OpenSSL 的加密模块
)
if(APPLE)
    set_target_properties(client_ssl PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

target_link_libraries(client
    PRIVATE
    absl::log
    absl::check
    absl::base
    absl::strings
    absl::raw_logging_internal
    absl::log_internal_message
    absl::spinlock_wait
    protobuf
    Threads::Threads
)
if(APPLE)
    set_target_properties(client PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

if(APPLE)
    set_target_properties(client_nb_connect PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()
