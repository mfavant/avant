# Avant

Network Message Framework For Linux C++.

CPP-MIN: `C++17`  
PLATFORM: `linux`  
PROTOCOL: `http` `tcp stream(protobuf)` `websocket`  
TLS-SSL: `openssl`  
SCRIPT: `lua`  

## Get Start

If there are already dependencies to be installed on the host, please selectively ignore them.

### Ubuntu (Docker)

```bash
$ docker run -it ubuntu
$ sudo apt-get update
$ sudo apt-get install apt-utils -y
$ sudo apt-get install cmake g++ make git -y
$ sudo apt-get install protobuf-compiler libprotobuf-dev -y
$ sudo apt-get install libssl-dev -y
$ git clone https://github.com/crust-hub/avant.git
$ cd avant
$ cd protocol
$ make
$ cd ..
$ cmake .
$ make -j4
```

### CentOS8 (Docker)

[centos8](./Centos8.md)

### Config File

```bash
$ sudo mkdir /avant_static
$ vim bin/config/main.ini
```

### Avant Start

```bash
$ cd bin
$ ./avant
$ ps -ef | grep avant
```

## Docker Example

```bash
$ docker run -it --privileged -p 20023:20023 -v ${LOCAL_HTTP_DIR_PATH}:/avant_static gaowanlu/avant:latest bash
$ cd ./bin
$ ./avant
```

## APP Example

support tcp keep-alive stream (protobuf) and http app (http-parser)、websocket

1. [framework config](https://github.com/crust-hub/avant/blob/main/bin/config/main.ini)
2. [stream protobuf app](https://github.com/crust-hub/avant/blob/main/src/app/stream_app.cpp)
3. [http app](https://github.com/crust-hub/avant/blob/main/src/app/http_app.cpp)
4. [websocket app](https://github.com/crust-hub/avant/blob/main/src/app/websocket_app.cpp)

## Response Per Second

CPU: Intel(R) Core(TM) i5-9600KF CPU @ 3.70 GHz   
OS : WSL2 Ubuntu Mem 8GB  (Windows 11)

```ini
config/main.ini 
    theads:6  
    max_conn:10000  
    accept_per_tick: 50  
```

wrk testing, avant http and node test/node_http_server.js .

```bash
# avant
$ wrk -c {{connection_num}} -t {{concurrency}} http://IP:20023/node_http_server.js
# nodejs
$ wrk -c {{connection_num}} -n {{concurrency}} http://IP:20025/node_http_server.js
```

```bash
# wrk report
# connection_num ./bin/avant    node node_http_server.js    all_bytes
```

## Third-Party

1、[@http-parser](https://github.com/nodejs/http-parser)  2、[@lua](https://github.com/lua/lua)  
3、[@protobuffer](https://github.com/protocolbuffers/protobuf)  4、[@openssl](https://github.com/openssl/openssl)  
5、[@zlib](https://github.com/madler/zlib)  
