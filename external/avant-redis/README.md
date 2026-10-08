# avant-redis

Thin Redis client wrapper over a vendored copy of
[hiredis](https://github.com/redis/hiredis) (`lib/hiredis/`, including the
SSL build).

**Not wired into avant's CMake build** — this directory is a standalone CMake
project (see `CMakeLists.txt`), built separately from the main tree.

## Components

- `include/redis.h` / `src/redis.cpp` — `avant::redis::redis(host, pwd,
  port = 6379)`. `test()` connects, `AUTH`s, `SELECT 0`s, does a string
  SET/GET, and round-trips a binary struct via `SET key %b` / `GET key`.
  Errors surface as `std::runtime_error`.
- `src/main.cpp` — demo executable; fill in the real host/password at the top
  before running.

## Build

```bash
cmake -S external/avant-redis -B external/avant-redis/build
cmake --build external/avant-redis/build -j4
# -> external/avant-redis/bin/avant-redis (demo) + avant_redis shared lib
```

Requires OpenSSL dev libraries (the hiredis SSL build links `ssl`/`crypto`).

## Notes

- `test()` is a demo, not an API. For real use, drive `redisContext` /
  `redisCommand` directly or extend the `redis` class.
- The wrapper only stores `host`/`pwd`/`port`; it opens and frees its
  connection inside `test()` — there is no persistent connection and no pool.
- `redisCommand` is blocking; call it from a thread that may block, or build
  on `hiredis`'s async API (`lib/hiredis/async.{c,h}` is included).
