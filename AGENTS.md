# AGENTS.md

Guidance for agents working in this repo. `CLAUDE.md` and `README.md` hold longer detail; this file lists only what is easy to miss or to guess wrong.

## Project

Modular C++20 network messaging framework: HTTP(S), WebSocket, TCP/UDP stream (Protobuf). Linux + macOS (Apple Silicon) ARM64. Each thread (main, worker, other) runs an isolated Lua VM with runtime hot-reload.

## Build

Order matters. Protobuf codegen and (for JIT) the LuaJIT static lib must exist before CMake runs.

```bash
cd protocol && make        # protoc -> protocol/proto_res/*.pb.{cc,h}
cd ..
# Only for -DAVANT_JIT_VERSION=ON: build external/LuaJIT-2.1 FIRST
#   macOS:  env MACOSX_DEPLOYMENT_TARGET=$(sw_vers -productVersion) make clean && make -j4
#   Linux:  make clean && make -j4
mkdir -p build && cd build && rm -rf *
cmake -DAVANT_JIT_VERSION=ON ..   # or OFF
make -j4
```

- `build.sh` runs the full flow but hardcodes `-DAVANT_JIT_VERSION=ON` and builds LuaJIT first. `clean.sh` reverts it.
- CMake outputs the binary to `bin/avant` and all `external/` shared libs to the source tree (see `external/CMakeLists.txt`).
- **CMake compiles with `-Wall -Werror`** — new warnings break the build. Fix warnings rather than suppressing them.
- **`AVANT_JIT_VERSION` is a global `#ifdef`**, not a CMake cache var. Toggle it by editing `CMakeLists.txt` (and `external/CMakeLists.txt`) or passing `-DAVANT_JIT_VERSION=ON`; a plain rebuild after a code change won't flip it.
- CMake `GLOB`s `src/**/*.cpp` and `protocol/*.pb.cc`, then `add_subdirectory(external)` builds the vendored `.so` libs. **New `.cpp` files are not picked up until you re-run `cmake`** (no `CONFIGURE_DEPENDS`).
- C++20, `CMAKE_CXX_EXTENSIONS OFF`.

### Dependencies (non-obvious)

- **macOS**: protobuf requires Abseil — `find_package(absl)` (and `absl::log check base strings raw_logging_internal log_internal_message spinlock_wait`). Build abseil with `-DCMAKE_POSITION_INDEPENDENT_CODE=ON`.
- Protobuf 3.11+ / 33.2, OpenSSL 1.1+ / 3.5.4, Node (for Lua codegen). See `install_third_party_example.md` for custom paths (`-DPROTOBUF_ROOT_DIR`, `-Dabsl_DIR`, `-DOPENSSL_ROOT_DIR`).
- `external/avant-redis`, `avant-sql`, `avant-libengine` exist but are **not** wired into the CMake build — do not assume they link.

## Lua codegen (regenerate after `.proto` changes)

```bash
make proto      # root Makefile: node ./generate_proto_lua.js ./protocol ./bin/lua/ProtoLua
```

Only regenerates `bin/lua/ProtoLua/*` (`ProtoLua_`-prefixed modules). Hand-editing those files is futile — fix the `.proto` and re-run. (The README's `node node` is a typo; use `make proto`.)

## Running

```bash
cd bin && ./avant
```

- SIGINT/SIGTERM = graceful shutdown; **SIGUSR1 = Lua hot-reload** (no restart).
- SIGPIPE/SIGCHLD are ignored at startup.
- Config is `bin/config/main.ini` (`[server]`, `[ipc]`, `[client]`); only **one** `task_type` (`HTTP_TASK`/`STREAM_TASK`/`WEBSOCKET_TASK`) is active at a time. Also sets `other_udp_svr_ip`/`other_udp_svr_port`/`other_udp_svr_max_loop` for the UDP server.

## Testing

No framework, no CI test job, not in CMake. `test/*.test.cpp` are compiled by hand:

```bash
g++ test/object_pool.test.cpp -o test/object_pool.exe -lpthread && ./test/object_pool.exe
g++ test/json.test.cpp external/avant-json/json.cpp external/avant-json/parser.cpp -o test/json.exe && ./test/json.exe
g++ external/avant-xml/element.cpp external/avant-xml/document.cpp test/xml.test.cpp -o test/xml.exe && ./test/xml.exe
g++ test/timer.test.cpp external/avant-timer/timer.cpp external/avant-timer/timer_manager.cpp -o test/timer.exe -I"src/" -lpthread && ./test/timer.exe
```

(`test/main.cpp` and `test/node_http_server.js` are support files, not tests.) There is no `make test`.

## Architecture (non-obvious)

- **Threading**: main thread accepts connections + owns IPC forwarding between workers and center; each worker has its own `event_poller` + connection set + isolated Lua VM; "other" thread handles UDP. Event loop is `epoll` (Linux) / `kqueue` (macOS) via `avant::event::event_poller`.
- **Lua entry**: `bin/lua/Init.lua` dispatches per-thread and loads `Main.lua`, `Worker.lua`, `Other.lua`, plus `Avant.lua`, `MapSvr.lua`, `Log.lua`, `PlayerData.lua`, `PlayerLogic.lua` and subdirs `Msg/`, `Time/`, `Numeric/`, `Algorithm/`, `Debug/`, `ProtoLua/`. Lifecycle hooks: `On{Main,Worker,Other}{Init,Tick,Stop}`.
- **Protocol buffer messages** live in `protocol/` (9 files: `proto_cmd`, `proto_err_code`, `proto_example`, `proto_ipc_stream`, `proto_lua`, `proto_message_head`, `proto_tunnel`, `proto_udp`, + `proto_res/` generated).
- **Connection contexts** in `src/connection/` (`http_ctx`, `stream_ctx`, `websocket_ctx`, `ipc_stream_ctx`, all over `base_ctx`); app handlers in `src/app/` (`http_app`, `stream_app`, `websocket_app`, `other_app`, `lua_plugin`).
- IPC is one-way, client-initiated TCP with app-id auth (`proto_ipc_stream.proto`); config in `bin/config/ipc.json`.

## Conventions

- C++ coding standards follow `isocpp.github.io` (see `.claude/skills/cpp-coding-standards`).
- `.luarc.json` ignores `src/`, `protocol/`, `build/`, `client/`, `external/`, `test/` from Lua tooling — those are not Lua sources.
- Docker/K8s deploy: `Dockerfile` builds **both** JIT=ON and OFF; CI is Docker-image-only (`.github/workflows/docker-image*.yml`), no test/lint job.
