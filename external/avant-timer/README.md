# avant-timer

Single-threaded timer library. A `timer` describes one schedule (expiry time,
repeat count, interval, callback); `timer_manager` keeps the live timers in an
expiration-ordered queue (`std::multimap`) plus an O(1) id index
(`std::unordered_map`) and fires whichever are due.

## Contract

- **One owning thread.** Call `add`, `check_and_handle`, and `mark_delete` from
  the same thread (e.g. the worker's event loop). Calling them from multiple
  threads on one manager is undefined behavior by design.
- **Callbacks must not touch the manager.** A callback may read state but must
  not `add`/`mark_delete` any timer (including re-adding itself).
  `check_and_handle` walks the queue while invoking callbacks and holds a cached
  iterator across the call; mutating the queue from a callback can invalidate it
  (undefined behavior). Do follow-up scheduling after the callback returns.
- **Unique id.** A `timer_id` must be unique per manager. Re-adding an existing
  id removes and replaces the old timer.
- **Clock units are up to you.** `now_time_stamp` and `interval` share the same
  unit (seconds in the worker). Pass the current time to `check_and_handle` to
  fire due timers.
- **Repeat semantics.** `repeated_times`: `REPEAT_INFINITE` (-1) fires forever;
  `1` fires once (one-shot); `n > 1` fires `n` times total, then is dropped.

## API

- `add(std::shared_ptr<timer>)` — insert/replace; returns `nullptr` on invalid args.
- `check_and_handle(uint64_t now)` — fire all timers due at `now`; re-arms
  repeating timers, drops one-shots.
- `mark_delete(uint64_t id)` — remove a timer without firing its callback.
- `size()` / `empty()` / `exist(id)`.

## Example

```cpp
#include <cstdint>
#include <cstdio>
#include <memory>
#include <chrono>
#include <thread>

#include <avant-timer/timer.h>
#include <avant-timer/timer_manager.h>

namespace
{
    uint64_t now_seconds()
    {
        using namespace std::chrono;
        return static_cast<uint64_t>(
            duration_cast<seconds>(steady_clock::now().time_since_epoch()).count());
    }
} // namespace

int main()
{
    avant::timer::timer_manager manager;
    uint64_t id = 0;

    // repeating: fires every 1 s, forever
    manager.add(std::make_shared<avant::timer::timer>(
        id++, now_seconds(), avant::timer::timer::REPEAT_INFINITE, 1,
        [](const avant::timer::timer &self)
        {
            std::printf("tick id %llu\n", static_cast<unsigned long long>(self.get_id()));
        }));

    // one-shot: fires once at now + 5 s
    manager.add(std::make_shared<avant::timer::timer>(
        id++, now_seconds(), 1, 5,
        [](const avant::timer::timer &self)
        {
            std::printf("done id %llu\n", static_cast<unsigned long long>(self.get_id()));
        }));

    // drive the manager from a single thread
    for (int i = 0; i < 10; ++i)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        manager.check_and_handle(now_seconds());
    }
    return 0;
}
```

Build from the repo root (`external/` is on the include path):

```bash
g++ -std=c++20 example.cpp external/avant-timer/timer.cpp external/avant-timer/timer_manager.cpp -Iexternal -lpthread -o timer.example.exe && ./timer.example.exe
```

## Tests

Deterministic unit tests (synthetic clock, no real sleep) live in
[`test/timer.test.cpp`](../../test/timer.test.cpp):

```bash
g++ -std=c++20 -Wall -Wextra -Werror test/timer.test.cpp \
    external/avant-timer/timer.cpp external/avant-timer/timer_manager.cpp \
    -o test/timer.exe -lpthread && ./test/timer.exe
```
