# avant-buffer

Growable byte buffer with independent read/write pointers, backed by one flat
allocation. Written bytes accumulate after the write pointer; consumed bytes
stay behind the read pointer until compacted or cleared.

Self-contained: no dependency on the rest of avant. Built as the shared lib
`avant-buffer` in `external/CMakeLists.txt` and linked into the avant binary.

## Contract

- **Soft cap `limit_max`.** The buffer starts at a 1024-byte capacity and
  grows (doubling) up to `limit_max`. `write()` throws `std::runtime_error`
  when the payload exceeds `limit_max`; `set_limit_max()` only grows (floor
  1024).
- **`write()` packs.** The live bytes are always contiguous. If the payload
  does not fit in the trailing space, the live bytes are `memmove`d to the
  front of the allocation first (growing it up to `limit_max` if still too
  small) and the payload then starts at the front; a payload that fits in the
  trailing space is simply appended behind any consumed front space.
- **`read()` is a best-effort copy.** It returns the number of bytes actually
  copied (possibly less than requested; 0 for null/zero args) and advances the
  read pointer.
- **Not thread-safe.** Synchronize externally; in avant each owner (e.g. a
  connection context) uses its own instance.

## API

- `write(source, size)` — append; returns `size`.
- `read(dest, size)` — copy up to `size` bytes out.
- `can_readable_size()` — bytes available to read.
- `copy_all(out, out_len)` — non-destructive snapshot; returns 0 if `out_len`
  is too small.
- `read_ptr_move_n(n)` — skip `n` bytes without copying; `false` if `n`
  exceeds the readable bytes.
- `force_get_read_ptr()` / `force_get_write_ptr()` — zero-copy access for
  serializing/parsing in place; lazily initializes an empty buffer.
- `blank_space()` — an upper bound on a single `write()`: consumed front space
  + trailing space + growth headroom up to `limit_max`. Any single write up to
  this size succeeds; writes beyond the consumed+trailing portion
  compact/grow the allocation, and writes beyond `limit_max` throw.
- `set_limit_max(n)` / `get_limit_max()` / `clear()`.

Move construction/assignment adopts the source's allocation (the source is
left in a valid, empty, re-usable state); copying copies the live bytes into a
fresh packed allocation.

## Example

```cpp
#include <cstdio>
#include <cstring>

#include <avant-buffer/buffer.h>

using avant::buffer::buffer;

int main()
{
    buffer buf;
    const char *msg = "hello world";
    buf.write(msg, std::strlen(msg));
    std::printf("readable: %zu\n", buf.can_readable_size());

    char out[64] = {0};
    std::printf("read: %zu\n", buf.read(out, sizeof(out)));
    return 0;
}
```

Build from the repo root (`external/` is on the include path):

```bash
g++ -std=c++20 example.cpp external/avant-buffer/buffer.cpp -Iexternal -o buffer.example.exe && ./buffer.example.exe
```

## Tests

[`test/buffer.test.cpp`](../../test/buffer.test.cpp) (roundtrip, partial reads,
growth/compaction, `limit_max`, move/copy semantics):

```bash
# from test/
g++ -std=c++20 buffer.test.cpp ../external/avant-buffer/buffer.cpp -o buffer.exe && ./buffer.exe
```
