# CLAUDE.md

This file provides guidance for Claude when working with code in this repository.

## Project Overview

Avant is a modular C++20 network messaging framework supporting HTTP(S), WebSocket, TCP Stream (Protobuf), and UDP (Protobuf) protocols. It runs on Linux (x86_64, ARM64) and macOS (Apple Silicon) ARM64. Each thread (main, worker, auxiliary) runs in its own isolated Lua virtual machine with optional LuaJIT support and runtime hot-reload via SIGUSR1.

## Directory Structure

- `src/` — Core framework source code
  - `system/` — System bootstrap (`system.cpp/h`, `config_mgr.cpp/h`)
  - `server/` — Server orchestration (`server.cpp/h`)
  - `event/` — Event loop abstraction (`event_poller.cpp/h`) — uses `epoll` on Linux, `kqueue` on macOS
  - `socket/` — Socket abstractions (`socket`, `server_socket`, `client_socket`, `socket_pair`)
  - `connection/` — Connection contexts per protocol (`connection`, `http_ctx`, `stream_ctx`, `websocket_ctx`, `ipc_stream_ctx`, `base_ctx`, `connection_mgr`)
  - `workers/` — Worker threads (`worker.cpp/h`, `other.cpp/h`)
  - `task/` — Task type dispatch (`task_type.cpp/h`)
  - `app/` — Application-level handlers per protocol (`http_app`, `stream_app`, `websocket_app`, `other_app`, `lua_plugin`)
  - `proto/` — Protocol buffer utilities (`proto_util.cpp/h`)
  - `utility/` — Shared utilities (singleton, object_pool, time)
  - `hooks/` — Signal handling and lifecycle (`init`, `reload`, `stop`, `tick`)
  - `global/` — Cross-cutting globals (`tunnel_id.cpp/h` — manages worker tunnel IDs for IPC)
  - `main.cpp` — Entry point: creates `avant::system::system`, calls `init()`
- `protocol/` — `.proto` definitions (`proto_cmd`, `proto_example`, `proto_ipc_stream`, `proto_lua`, `proto_message_head`, `proto_tunnel`), compiled output to `protocol/proto_res/`
- `external/` — Third-party libraries built as shared objects: `llhttp`, `avant-buffer`, `avant-inifile`, `avant-ipc` (UDP component), `avant-json`, `avant-log`, `avant-timer`, `avant-xml`, `avant-zlib`, `avant-libengine`, `avant-redis`, `avant-sql`, `lua` (Lua 5.5), `LuaJIT-2.1.ROLLING`, `zlib`
  - Note: `avant-redis` and `avant-sql` are present but not currently wired into the CMake build.
- `test/` — Standalone test files (compiled manually, not integrated into CMake)
- `bin/` — Output directory for the `avant` binary and runtime config (`bin/config/main.ini`, `bin/lua/Init.lua`, `bin/config/ipc.json`)
- `client/` — Client implementations (C++, JavaScript/WebSocket, Node.js, Go)

## Build Instructions

### Prerequisites

- CMake 3.10+
- C++20 compiler (g++ or clang++)
- Protobuf 3.11+ (`protoc`, `libprotobuf-dev`) — also tested with 33.2
- OpenSSL 1.1+ (`libssl-dev`) — also tested with 3.5.4
- On macOS: `absl` (Abseil, via Homebrew or custom build) — required by protobuf on Apple platforms; must be built with `-DCMAKE_POSITION_INDEPENDENT_CODE=ON`

### Build Steps

```bash
# 1. Generate protobuf files
cd protocol && make && cd ..

# 2. Configure and build
mkdir -p build && cd build
rm -rf *
cmake -DAVANT_JIT_VERSION=ON ..   # or OFF for Lua 5.5 (default)
make -j4
```

- `AVANT_JIT_VERSION=ON` — compile with LuaJIT (requires building LuaJIT from `external/LuaJIT-2.1.ROLLING`)
- `AVANT_JIT_VERSION=OFF` — compile with standard Lua 5.5 from `external/lua`

Binary is output to `bin/avant`.

### Custom Dependency Paths

When dependencies are installed to non-standard locations, pass CMake variables:

```bash
cmake \
  -DPROTOBUF_ROOT_DIR=/path/to/protobuf \
  -Dabsl_DIR=/path/to/abseil/lib64/cmake/absl \
  -DOPENSSL_ROOT_DIR=/path/to/openssl \
  ..
```

See `install_third_party_example.md` for full examples (Abseil 20250512, Protobuf 33.2, OpenSSL 3.5.4).

### Run

```bash
cd bin
./avant
# Stop: kill -SIGTERM <PID>
# Lua hot-reload: kill -SIGUSR1 <PID>
```

## Architecture

### Threading Model

The server uses a multi-threaded architecture:

1. **Main thread** — Accepts incoming connections, manages worker tunnels via `socket_pair` and `tunnel_id` global, handles IPC forwarding between workers and the center (main thread). Owns `event_poller` for listening socket.
2. **Worker threads** — Each worker has its own `event_poller` instance and connection set. Handles application-level protocol dispatch (HTTP, stream, websocket). Each worker gets an isolated Lua VM.
3. **Other thread** — Handles UDP server functionality and auxiliary tasks.

### Event Loop

`avant::event::event_poller` abstracts the platform-specific I/O multiplexing:
- Linux: `epoll` (LT/ET mode support)
- macOS: `kqueue` (with proper `timespec` timeout handling; tracks registered events per FD to avoid duplicate registrations)

Each thread (main, workers, other) owns its own `event_poller` instance.

### Connection Contexts

Protocol-specific state lives in context classes under `src/connection/`:
- `http_ctx` — HTTP request/response parsing
- `stream_ctx` — TCP Protobuf stream handling
- `websocket_ctx` — WebSocket frame handling
- `ipc_stream_ctx` — IPC stream connection

All inherit from or compose `base_ctx`. Connections are managed by `connection_mgr`.

### Global State

- `avant::global::tunnel_id` — Singleton managing worker tunnel ID allocation and ranges for IPC (`src/global/tunnel_id.cpp/h`)

### Lua Integration

The `lua_plugin` class manages Lua VMs per thread:
- Entry script: `bin/lua/Init.lua` (which loads `Main.lua`, `Worker.lua`, `Other.lua`, `Log.lua`, `PlayerData.lua`, `PlayerLogic.lua`)
- Lifecycle hooks: `OnMainInit`, `OnMainTick`, `OnMainStop`, `OnWorkerInit`, `OnWorkerTick`, `OnWorkerStop`, `OnOtherInit`, `OnOtherTick`, `OnOtherStop`
- Hot-reload via `SIGUSR1` triggers `exe_OnLuaVMReload` across all VMs
- Bridge functions: `Logger`, `Lua2Protobuf`, `CreateNewProtobufByCmd`, `HighresTime`, `Monotonic`
- Message factory maps protobuf command IDs to message constructors, enabling Lua to send/receive Protobuf messages

### Protocol Buffer Messages

Key protobuf definitions in `protocol/`:
- `proto_message_head` — Message header (command id, sequence, length)
- `proto_cmd` — Command definitions
- `proto_tunnel` — Tunnel/IPC package (includes `ProtoTunnelID` ranges for worker tunnels)
- `proto_lua` — Lua bridge message types
- `proto_ipc_stream` — IPC stream auth handshake
- `proto_example` — Example messages

Generated files go to `protocol/proto_res/` and are compiled into the main binary.

### Task Types

`avant::task::task_type` enum: `HTTP_TASK`, `STREAM_TASK`, `WEBSOCKET_TASK`, `NONE`. Set in `bin/config/main.ini` under `[server].task_type`. Only one task type is active at a time.

## Configuration

`bin/config/main.ini` sections:
- `[server]` — `app_id`, `ip`, `port`, `worker_cnt`, `max_client_cnt`, `epoll_wait_time`, `accept_per_tick`, `task_type`, `http_static_dir`, `lua_dir`, SSL cert/key paths, `use_ssl`, `daemon`, `log_level`
- `[ipc]` — `max_ipc_conn_num`, `ipc_json_path`
- `[client]` — `threads`

Other config files in `bin/config/`:
- `ipc.json` — IPC connection configuration
- `workflow.xml` — Workflow definition
- `ssl_cfg.sh` — SSL certificate helper script

## Testing

Tests are standalone `.test.cpp` files in `test/` (json, object_pool, timer, xml). They are not integrated into CMake. Compile manually:

```bash
# Example: object pool test
g++ test/object_pool.test.cpp -o test/object_pool.exe -lpthread
./test/object_pool.exe

# Example: JSON test
g++ test/json.test.cpp external/avant-json/json.cpp external/avant-json/parser.cpp -o test/json.exe

# Example: XML test
g++ external/avant-xml/element.cpp external/avant-xml/document.cpp test/xml.test.cpp -o test/xml.exe

# Example: timer test
g++ test/timer.test.cpp external/avant-timer/timer.cpp external/avant-timer/timer_manager.cpp -o test/timer.exe -I"src/" -lpthread
```

## Key Implementation Details

- `avant::utility::singleton` — Thread-safe singleton template used throughout (e.g., `logger::instance()`)
- `avant::utility::object_pool` — Pooled memory allocator for frequently allocated/freed objects
- Signal handling: SIGPIPE/SIGCHLD ignored, SIGINT/SIGTERM trigger graceful shutdown, SIGUSR1 triggers Lua hot-reload
- Stack buffers have been migrated to `std::vector<char>` (e.g., in HTTP static file response) to prevent stack overflow on large files
- On macOS, protobuf depends on Abseil (`absl`) — ensure it's installed via Homebrew or custom build with PIC enabled
- Docker builds are automated via GitHub Actions (`docker-image.yml`, `docker-image-pullrequest.yml`)
