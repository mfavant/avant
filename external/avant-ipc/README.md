# avant-ipc

Inter-process / inter-module communication components for avant: UDP, POSIX
shared-memory pool, and FIFO pair.

Status:

| component    | state      | built into avant |
| ------------ | ---------- | ---------------- |
| `udp_component` | production | yes — `external/CMakeLists.txt` builds it as the `avant-ipc` shared lib, used by `src/workers/other.*` (UDP server) and `src/app/other_app.cpp` / `lua_plugin.cpp` |
| `shm_pool`   | development| no (header-only template) |
| `fifo`       | development| no (header-only class) |

This module is self-contained: no dependency on the rest of avant.

## udp_component (`udp_component.h` / `udp_component.cpp`)

UDP socket wrapper with dual event-loop support:

- `udp_component_server(IP, PORT, start_event_loop)` — creates a socket
  (IPv6 dual-stack when `IP` is `""` or an IPv6 literal) and binds it.
  - `start_event_loop=true`: runs a blocking `event_loop()` (epoll on Linux /
    kqueue on macOS, edge-triggered, 1s timeout); exit via `tick_callback`.
  - `start_event_loop=false`: returns after bind so the caller can register
    `get_socket_fd()` into *its own* event poller and call `server_recvfrom()`
    on readable events (this is how the avant "other" worker uses it).
- `udp_component_client(TARGET_IP, TARGET_PORT, buffer, len, addr, addr_len)` —
  send one UDP datagram. With `addr != nullptr` it replies to the given address
  (server → client echo); otherwise it sends to `TARGET_IP:TARGET_PORT` and
  returns (no event loop). If the socket does not exist yet it is created
  automatically using the family of `addr`/`TARGET_IP`.
- `server_recvfrom(max_loop)` — drains up to `max_loop` datagrams with a
  reusable 64 KiB buffer and dispatches each to `message_callback(buffer, len,
  const sockaddr_storage&, socklen_t)`.
- Callbacks: `tick_callback(bool &to_stop)`, `message_callback(...)`,
  `close_callback()`.
- `udp_component_setnonblocking(fd)` — public helper (also used by avant).

Caveats:

- The socket family is decided when the socket is first created and cannot
  change afterwards: a client socket created for IPv4 cannot echo to an IPv6
  peer (bind the server dual-stack, i.e. with `IP = ""`, to accept both).
- The internal event loop registers the socket into its *own* epoll/kqueue;
  when `start_event_loop=false` the external owner is responsible for the fd.

Tests (ping-pong, server on 127.0.0.1:20027), in `test/`:

```bash
# from test/
g++ -std=c++20 udp_server.test.cpp ../external/avant-ipc/udp_component.cpp -o udp_server.exe && ./udp_server.exe
g++ -std=c++20 udp_client.test.cpp ../external/avant-ipc/udp_component.cpp -o udp_client.exe && ./udp_client.exe
```

## shm_pool (`shm.h`, header-only)

Fixed-size pool of `T` objects in a POSIX shared memory object (`shm_open`):

```
[ header: magic/version/count/mem_size ][ use-list: 1 byte/slot ][ pad ][ count * T ]
```

```cpp
#include <avant-ipc/shm.h>

avant::ipc::shm_pool<MyStruct> pool("/my_pool", 256);
pool.init();              // create + map, or open an existing one
MyStruct *s = pool.alloc(); // nullptr when exhausted
...
pool.back(s);
pool.unlink();            // free the shm object on clean shutdown
```

Notes:

- `init()` verifies the header of an existing object and rejects it when the
  magic/version/count/size do not match (stale object from an old
  configuration) — call `unlink()` first in that case.
- A crashed creator leaves the object behind with stale "in-use" slots; call
  `reset()` after `init()` to free all slots again.
- The kernel may round the object size up to a page (16384 on Apple Silicon);
  that is expected and handled.
- Slot claiming (`alloc`/`back`) is atomic, so concurrent processes may claim
  slots safely. The *payload* of a slot is NOT synchronized — order
  producer/consumer accesses yourself (e.g. only write after `alloc`, only
  read after the owner `back`ed it, plus your own hand-shake).
- `foreach (callback, used)` visits in-use (default) or free slots.

Test:

```bash
# from test/  (Linux: add -lrt)
g++ -std=c++20 shm.test.cpp -o shm_test.exe && ./shm_test.exe
```

## fifo (`fifo.h`, header-only)

Full-duplex channel between exactly two peers, built from a pair of POSIX
fifos (`a2b_path`, `b2a_path`). Peer `AUTH_A` writes a2b / reads b2a,
`AUTH_B` the reverse.

- `init()` opens both fifos with `O_RDWR | O_NONBLOCK`, so it never blocks
  waiting for the other peer; the peer may come up later. Throws
  `std::runtime_error` on failure.
- Both fds are non-blocking: `write()` returns `-1` (`errno = EAGAIN` when the
  pipe buffer is full, `EPIPE` when the peer vanished) — retry from your event
  loop instead of blocking. `recv()` returns the byte count read, `0` when
  nothing is available, `-1` on error.
- Because the fds are `O_RDWR`, a peer closing its side is *not* visible as
  `read() == 0`; detect shutdown out-of-band (or via `EPIPE` on write).
- The destructor unlinks both fifo paths and invokes `destroy_callback`.

Demo (two in-process threads, one second apart):

```bash
# from test/
g++ -std=c++20 fifo.test.cpp -o fifo.exe -lpthread && ./fifo.exe
```
