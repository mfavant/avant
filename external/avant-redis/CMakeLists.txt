cmake_minimum_required(VERSION 3.10.0)
project(avant-redis)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++2a -Wall -O0")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories(${PROJECT_SOURCE_DIR}/include/)
include_directories(${PROJECT_SOURCE_DIR}/lib/hiredis)

file(GLOB_RECURSE SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/*.cpp")

list(REMOVE_ITEM SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/main.cpp")

set(HIREDIS_SOURCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/hiredis)

message(${HIREDIS_SOURCE})

add_library(avant_redis SHARED ${SOURCE_FILES} ${HIREDIS_SOURCE})

add_executable(${PROJECT_NAME} src/main.cpp ${SOURCE_FILES} ${HIREDIS_SOURCE})

target_link_libraries(${PROJECT_NAME} "ssl" "crypto")
