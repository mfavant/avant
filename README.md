# Avant

[![Docker Image CI](https://github.com/mfavant/avant/actions/workflows/docker-image.yml/badge.svg)](https://github.com/mfavant/avant/actions/workflows/docker-image.yml)

Network Message Framework For Linux C++.

C++ Version: `C++20`  
OS: `Linux`  
Protocol: `HTTP` `TCP Stream(protobuf)` `Websocket`  
TLS/SSL: `OpenSSL`  
Script Engin: `Lua 5.4.6`  

## Docker Image

```bash
docker run --privileged -p 20023:20023 -v ${LOCAL_HTTP_DIR_PATH}:/avant_static gaowanlu/avant:latest
```

## Get Start

If there are already dependencies to be installed on the host, please selectively ignore them.

### Ubuntu (Docker)

```bash
docker run -it ubuntu
sudo apt-get update
sudo apt-get install apt-utils -y
sudo apt-get install cmake g++ make git -y
sudo apt-get install protobuf-compiler libprotobuf-dev -y
sudo apt-get install libssl-dev -y
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

CentOS8 (Docker)

[centos8](./centos8.md)

### Config File

```bash
sudo mkdir /avant_static
vim bin/config/main.ini
```

### Start

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

## App-Layer

support tcp keep-alive stream (protobuf) and http app (http-parser)、websocket

1. [framework config](https://github.com/crust-hub/avant/blob/main/bin/config/main.ini)
2. [stream protobuf app](https://github.com/crust-hub/avant/blob/main/src/app/stream_app.cpp)
3. [http app](https://github.com/crust-hub/avant/blob/main/src/app/http_app.cpp)
4. [websocket app](https://github.com/crust-hub/avant/blob/main/src/app/websocket_app.cpp)

## Client

- [C++](client/cpp/)
- [Websocket](client/websocket/)
- [Node.js](client/javascript/)
- [Go](client/go/avant/)

## Lua

The main thread, other thread, and each worker thread have their own independent Lua virtual machine. `config/Init.lua`

### Lua Hot Update

Using signal SIGUSR1(10), Avant provides Lua hot updates without restarting the process.

```bash
ps -ef | grep avant
kill -10 PID
```

## IPC

The configuration file is located at `config/ipc.json`. Adopt one-way TCP active connection. Authentication handshake is verified through the `appid` content in [ProtoIPCStreamAuthHandshake protocol](./protocol/proto_ipc_stream.proto).

## Wrk

[wrk tool](https://github.com/wg/wrk)

```bash
# avant
wrk -c {{connection_num}} -t {{threads}} http://IP:20023/
wrk -c {{connection_num}} -t {{threads}} -d60s --header "Connection: keep-alive" http://127.0.0.1:20023/
```

## Third-Party

1、[@nodejs/http-parser](https://github.com/nodejs/http-parser)  2、[@lua/lua](https://github.com/lua/lua)  
3、[@protocolbuffers/protobuf](https://github.com/protocolbuffers/protobuf)  4、[@openssl/openssl](https://github.com/openssl/openssl)  
5、[@madler/zlib](https://github.com/madler/zlib)  6、[@homer6/url](https://github.com/homer6/url)

