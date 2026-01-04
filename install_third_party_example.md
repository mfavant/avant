## Install Third-Party Example

## protobuf3

```bash
wget https://github.com/protocolbuffers/protobuf/releases/download/v3.11.2/protobuf-all-3.11.2.tar.gz
tar zxf protobuf-all-3.11.2.tar.gz
cd protobuf-3.11.2
./configure -prefix=/usr/local/
sudo make -j4 && sudo make install
protoc --version
```

## CentOS8

```bash
docker run -it centos
cd /etc/yum.repos.d/
sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-*
sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*
yum install -y epel-release
yum install cmake gcc gcc-c++ make git -y
yum install openssl-devel -y
dnf --enablerepo=powertools install protobuf -y
dnf --enablerepo=powertools install protobuf-devel -y
git clone https://github.com/crust-hub/avant.git
cd avant
cd protocol
make
cd ..
mkdir build
rm -rf ./build/*
cd build
cmake ..
make -j4
```

## Install Protobuf-33.2

```bash
wget https://github.com/protocolbuffers/protobuf/releases/download/v33.2/protobuf-33.2.zip
cd protobuf-33.2
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/data/home/myuser/local/protobuf -DCMAKE_BUILD_TYPE=Release -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_BUILD_SHARED_LIBS=ON -Dprotobuf_ABSL_PROVIDER=package
make -j5
make install
cd /data/home/myuser/local/protobuf
ls
```

## Install Abseil-Cpp Protobuf OpenSSL 

abseil-cpp-20250512

```bash
https://github.com/abseil/abseil-cpp/releases/download/20250512.0/abseil-cpp-20250512.0.tar.gz
解压后 mkdir build , cd build
cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/data/home/myuser/local/abseil
make -j5
make install 到 /data/home/myuser/local/abseil
```

protobuf-33.2

```bash
https://github.com/protocolbuffers/protobuf/releases/download/v33.2/protobuf-33.2.zip
cd /data/home/myuser/protobuf
cmake .. \
  -DCMAKE_INSTALL_PREFIX=/data/home/myuser/local/protobuf \
  -DCMAKE_BUILD_TYPE=Release \
  -Dprotobuf_BUILD_TESTS=OFF \
  -Dprotobuf_BUILD_SHARED_LIBS=ON \
  -Dprotobuf_ABSL_PROVIDER=package \
  -Dabsl_DIR=/data/home/myuser/local/abseil-20250512/lib64/cmake/absl
```

openssl-3.5.4

```bash
https://github.com/openssl/openssl/releases/download/openssl-3.5.4/openssl-3.5.4.tar.gz
./Configure \
--prefix=/data/home/myuser/local/openssl-3.5.4 \
--openssldir=/data/home/myuser/local/openssl-3.5.4/ssl \
shared \
linux-x86_64

可能有其他依赖，perl的，一般可以yum或apt直接安装
```

指定Protobuf  OpenSSL目录样例

```cmake
cmake_minimum_required(VERSION 2.8)
project(avant)

# 设置 Protobuf 的查找路径
set(PROTOBUF_ROOT_DIR /data/home/myuser/local/protobuf)
list(APPEND CMAKE_PREFIX_PATH /data/home/myuser/local/protobuf)
# 查找 Protobuf
find_package(Protobuf 3.21.0 REQUIRED)

if (Protobuf_FOUND)
    message(STATUS "Protobuf found at ${Protobuf_INCLUDE_DIRS}")
    include_directories(${Protobuf_INCLUDE_DIRS})
endif()

# 设置 Abseil 的查找路径
set(absl_DIR /data/home/myuser/local/abseil-20250512/lib64/cmake/absl)

find_package(absl 20250512 REQUIRED)
if (absl_FOUND)
    message(STATUS "absl found at ${absl_INCLUDE_DIRS}")
    include_directories(${absl_INCLUDE_DIRS})
endif()

# 设置 OpenSSL 的查找路径
set(OPENSSL_ROOT_DIR /data/home/myuser/local/openssl-3.5.4)
list(APPEND CMAKE_PREFIX_PATH /data/home/myuser/local/openssl-3.5.4)

# 查找 OpenSSL
find_package(OpenSSL 1.1 REQUIRED)
if (OpenSSL_FOUND)
    message(STATUS "OpenSSL found at ${OpenSSL_INCLUDE_DIRS}")
    include_directories(${OpenSSL_INCLUDE_DIRS})
endif()


set(CMAKE_C_COMPILER /data/home/myuser/local/gcc-15.2.0/bin/gcc)
set(CMAKE_CXX_COMPILER /data/home/myuser/local/gcc-15.2.0/bin/g++)
include_directories(
    /usr/include
    /data/home/myuser/local/gcc-15.2.0/include
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
link_directories(
    /data/home/myuser/local/gcc-15.2.0/lib64
    /data/home/myuser/local/gcc-15.2.0/lib
    /usr/lib
    /usr/lib64
    /data/home/myuser/local/abseil-20250512/lib64
)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Werror -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -g -Wall -Werror -O0")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

message(${PROJECT_SOURCE_DIR}/src)


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

# 使用找到的库文件
target_link_libraries(${PROJECT_NAME}
    pthread 
    dl
    absl::log #使用absl静态库 protobuf依赖absl
    absl::check
    absl::base
    absl::strings
    absl::raw_logging_internal
    absl::log_internal_message
    absl::spinlock_wait
    protobuf::libprotobuf
    OpenSSL::SSL  # 使用 OpenSSL 的模块
    OpenSSL::Crypto  # 使用 OpenSSL 的加密模块
    ${EXTERNAL_LIB}
)

install(TARGETS ${PROJECT_NAME} DESTINATION /usr/bin)

```
