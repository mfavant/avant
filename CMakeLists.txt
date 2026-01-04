cmake_minimum_required(VERSION 3.10.0)
project(avant)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Werror -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Werror -O0")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

message(${PROJECT_SOURCE_DIR}/src)

find_package(Protobuf 3.0.0 REQUIRED)
if (Protobuf_FOUND)
    message(STATUS "PROTOBUF_ROOT_DIR: ${PROTOBUF_ROOT_DIR}")
    message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
    message(STATUS "Protobuf found at ${Protobuf_INCLUDE_DIRS}")
    include_directories(${Protobuf_INCLUDE_DIRS})
endif()

find_package(OpenSSL 1.1 REQUIRED)
if (OpenSSL_FOUND)
    message(STATUS "OPENSSL_ROOT_DIR: ${OPENSSL_ROOT_DIR}")
    message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
    message(STATUS "OpenSSL found at ${OpenSSL_INCLUDE_DIRS}")
    include_directories(${OpenSSL_INCLUDE_DIRS})
endif()

# add include for src/
include_directories(${PROJECT_SOURCE_DIR}/src)

# add include for protocol/
include_directories(${PROJECT_SOURCE_DIR}/protocol)

# add include for external/
include_directories(${PROJECT_SOURCE_DIR}/external)

# for cpp file
file(GLOB_RECURSE SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/*.cpp")

# for protocol cc file
file(GLOB_RECURSE SOURCE_PROTOCOL_FILES "${PROJECT_SOURCE_DIR}/protocol/*.pb.cc")
list(APPEND SOURCE_FILES ${SOURCE_PROTOCOL_FILES})

# for plugin
# file(GLOB_RECURSE SOURCE_PLUGIN "${PROJECT_SOURCE_DIR}/src/plugin/*.cpp")
# list(REMOVE_ITEM SOURCE_FILES ${SOURCE_PLUGIN})
# foreach(item ${SOURCE_FILES})
# message("SourceFile: ${item}")
# endforeach()
add_executable(${PROJECT_NAME} src/main.cpp ${SOURCE_FILES})
target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unused-variable)

# for src/plugin CMakeLists.txt
# add_subdirectory(${PROJECT_SOURCE_DIR}/src/plugin)

# for external/ CMakeLists.txt
add_subdirectory(${PROJECT_SOURCE_DIR}/external)

# for link external gen .so lib
set(EXTERNAL_LIB avant-llhttp avant-inifile avant-log avant-ipc-udp avant-timer avant-xml avant-buffer avant-lua avant-zlib avant-json)

target_link_libraries(${PROJECT_NAME} 
    pthread 
    dl 
    protobuf::libprotobuf
    OpenSSL::SSL 
    OpenSSL::Crypto
    ${EXTERNAL_LIB}
)

install(TARGETS ${PROJECT_NAME} DESTINATION /usr/bin)
