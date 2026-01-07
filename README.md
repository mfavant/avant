# Avant

[![Docker Image CI](https://github.com/mfavant/avant/actions/workflows/docker-image.yml/badge.svg)](https://github.com/mfavant/avant/actions/workflows/docker-image.yml)

A High-Performance Network Messaging Framework for Linux C++.

- Language: `C++ 20`
- Platform: `Linux`
- Protocols: `HTTP(S) | TCP Stream(Protobuf) | WebSocket | UDP(Protobuf)`
- TLS/SSL: `OpenSSL`
- Lua: `Lua 5.4.8 | LuaJIT-2.1.ROLLING` 

## Overview

Avant is a modular and extensible network framework for Linux, designed to serve as a foundation
for building customized network services rather than providing fixed, opinionated solutions.

It can be used to implement HTTP servers, WebSocket-based services, and high-performance TCP or UDP
communication using Protocol Buffers. Avant also provides an IPC mechanism based on TCP and Protobuf
for internal service communication.

A practical example built on top of Avant is
[mapsvr](https://github.com/gaowanlu/mapsvr), a game server framework that supports
TCP Protobuf, WebSocket Protobuf, UDP Protobuf, and IPC TCP Protobuf,
with Lua scripting and hot-reload support.

Avant focuses on providing core networking components and infrastructure.
Application-level logic, protocols, and workflows are intentionally left to the user,
allowing maximum flexibility and customization.

Avant is particularly suitable for developers who want to build customized and experimental
network applications in C++ without relying on large, opinionated frameworks such as Boost or
event-driven libraries like libevent. Its clear and approachable codebase exposes core networking
concepts directly, making it easier to design application-specific protocols and workflows.

For C++ beginners, Avant can also serve as a practical learning project for understanding
Linux system programming, network I/O, and multi-threaded server architecture.

## Docker Image

```bash
docker run --privileged \
    -p 20023:20023 \
    -v ${LOCAL_HTTP_DIR_PATH}:/avant_static \
    gaowanlu/avant:latest
```

## Getting Started

If some dependencies are already installed on the host, you may skip them.

### Ubuntu Example

```bash
docker run -it ubuntu:latest bash

apt-get update

apt-get install -y \
    apt-utils \
    cmake g++ make git \
    protobuf-compiler libprotobuf-dev \
    libssl-dev

git clone https://github.com/crust-hub/avant.git

cd avant/protocol && make

cd ..

mkdir -p build
rm -rf ./build/*

cd build
cmake ..
make -j4
```

See: [Other Installing Third-Party Example](./install_third_party_example.md)

### Configuration

```bash
sudo mkdir /avant_static
vim bin/config/main.ini
```

### Running

```bash
cd bin
./avant
ps -ef | grep avant
```

### Stop

```bash
ps -ef | grep avant
kill PID
```

## Application Layer

Avant supports HTTP, WebSocket, UDP, and TCP streaming (Protobuf):

1. [framework config](bin/config/main.ini)
2. [tcp_stream protobuf app](src/app/stream_app.cpp)
3. [http app](src/app/http_app.cpp)
4. [websocket app](src/app/websocket_app.cpp)
5. [other_app::on_udp_server_recvfrom](src/app/other_app.cpp)

## Client Implementations

- [C++](client/cpp/)
- [WebSocket](client/websocket/)
- [Node.js](client/javascript/)
- [Go](client/go/avant/)

## Lua

Each thread (main, worker, auxiliary) runs in its own isolated Lua virtual machine.

* Entry script: `bin/lua/Init.lua`

### Lua Hot Reload

Lua scripts can be reloaded at runtime using SIGUSR1 without restarting the process:

```bash
ps -ef | grep avant
kill -USR1 <PID>
```

## IPC

* Configuration: `config/ipc.json`
* One-way, client-initiated TCP connection
* Authentication via appid in [ProtoIPCStreamAuthHandshake protocol](./protocol/proto_ipc_stream.proto)

## Benchmark

Using [wrk](https://github.com/wg/wrk)

```bash
wrk -c {{connection_num}} -t {{threads}} -d60s \
    --header "Connection: keep-alive" \
    http://127.0.0.1:20023/
```

## Third-Party Dependencies

1. [@nodejs/llhttp](https://github.com/nodejs/llhttp)  
2. [@lua/lua](https://github.com/lua/lua)  
3. [@protocolbuffers/protobuf](https://github.com/protocolbuffers/protobuf)  
4. [@openssl/openssl](https://github.com/openssl/openssl)  
5. [@madler/zlib](https://github.com/madler/zlib)  
6. [@homer6/url](https://github.com/homer6/url)

