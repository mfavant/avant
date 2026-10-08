# avant-log

Synchronous, file-based logger: a process-wide singleton (Meyers), serialized
by an internal mutex, formatted with C++20 `std::format`. Requires
`-std=c++20`. Self-contained. Built as the shared lib `avant-log` in
`external/CMakeLists.txt` and used across `src/` via the `LOG_*` macros.

## Usage

```cpp
#include <avant-log/logger.h>

// once at startup: mkdir -p the path and open the current hour's file
avant::log::logger::instance().open("logs", avant::log::logger::INFO);

LOG_INFO("worker {} started", id);            // C++20 format strings
LOG_ERROR("bind {} failed: {}", ip, errno);
ASSERT_LOG_EXIT(ptr != nullptr);              // LOG_FATAL + exit(-1)
```

`open(base_path, level)` returns `0` on success, `-1` if the directory/file
could not be created or opened.

Standalone build from the repo root (`external/` is on the include path;
requires C++20 for `std::format`):

```bash
g++ -std=c++20 example.cpp external/avant-log/logger.cpp -Iexternal -lpthread -o log.example.exe && ./log.example.exe
```

## Behavior

- **Levels** `DEBUG < INFO < WARN < ERROR < FATAL`. `open()` sets the minimum
  level; lower-severity calls return after a cheap atomic read, before
  formatting.
- **Hourly rotation.** One file per hour: `base_path/YYYY-MM-DD_HH.log`,
  opened in append mode. Rotation is checked on every write; a failed `fopen`
  is accounted for that hour (bounded to one retry per hour) instead of
  retried on every message.
- **Line format:** `YYYY-MM-DD HH:MM:SS  LEVEL  file:line func  message`.
- **No exit on the write path; file I/O failures do not throw.** If the file
  is not open the message is dropped, with a stderr notice on every drop
  (`std::format` can still raise `std::format_error` for a bad format string
  or an unformattable argument). A failed `open()` leaves the logger usable;
  the next successful `open()` recovers it.
- **Synchronous.** Each `LOG_*` takes the mutex, writes one line, and
  `fflush`es — no background thread. Concurrency is safe; throughput is not.
- The singleton's destructor closes the file; `open()` closes any previous
  file first. No explicit close is needed.

## Tests

Deterministic unit tests in [`test/log.test.cpp`](../../test/log.test.cpp)
(throwaway temp dir; open → log → close → reopen; level filtering; no spurious
in-hour rotation; 8-thread concurrent writes with no torn lines; the no-exit
contract):

```bash
g++ -std=c++20 -Wall -Wextra -Werror test/log.test.cpp external/avant-log/logger.cpp -o test/log.exe -lpthread && ./test/log.exe
```
