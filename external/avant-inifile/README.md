# avant-inifile

Lenient INI file parser: `[section]` tags, `key = value` lines, `#` comments.
Self-contained. Built as the shared lib `avant-inifile` in
`external/CMakeLists.txt` and used by `src/system/config_mgr` to load
`bin/config/main.ini`.

## Parsing

- `load(filename)` returns `false` only when the file cannot be opened.
  Unparseable lines (an unclosed section tag, a key outside any section, an
  empty tag) are skipped with a warning on stderr; the load still succeeds.
  Validating parsed values is the caller's job.
- `save(filename)` writes each section as a `[name]` block of `key = value`
  lines (alphabetical — `std::map` order).
- `clear()` drops everything (and the stand-in section below).

## inifile

- `inifile(filename)` / `load(filename)` / `save(filename)`
- `get(section, key)` — `const`. Returns a `const value &`; a missing section
  or key yields a shared default-constructed (empty) value. The reference is
  `const`, so writing through it is impossible.
- `set(section, key, value)` — the one write path; creates the section if
  needed. The stored text is the string form.
- `has(section)` / `has(section, key)` / `remove(section)` / `remove(section, key)`
- `operator[](section)` — an existing section is returned by reference. A
  missing section yields a per-instance stand-in map: writes land there, are
  **never** persisted by `save()`, and are wiped on the next `load()`/`clear()`
  — use `set()` to store values. Note that on an *existing* section,
  `ini["s"]["k"]` still default-constructs a missing key (`std::map`
  `operator[]` semantics) and that key *is* persisted.
- `operator<<(std::ostream&)` — dump for debugging.

## value

A string that can be interpreted as `bool`, `int`, `double`, or `string`
(constructors, assignment, and conversion operators for each):

- `bool` — case-insensitive: `1`/`2`/`true`/`yes`/`on` are truthy, anything
  else (including `0`/`false`/`no`/`off`/garbage/empty) is falsy.
- `int` — saturates to `INT_MIN`/`INT_MAX` on overflow, `0` if unparseable.
- `double` — clamps to `±DBL_MAX` (including non-finite input), `0.0` if
  unparseable.
- `==` — numeric strings compare numerically (`"3" == "3.0"`), values from the
  bool set compare as booleans, everything else compares by text.

## Example

```cpp
#include <cstdio>

#include <avant-inifile/inifile.h>

using namespace avant::inifile;

int main()
{
    inifile ini;                               // or: inifile ini("main.ini");

    ini.set("server", "port", "8080");
    ini.set("server", "enable", "true");
    ini.save("main.ini");

    const int port = ini.get("server", "port");       // int conversion
    const bool enable = ini.get("server", "enable");  // bool conversion
    std::printf("port=%d enable=%d\n", port, static_cast<int>(enable));
    return 0;
}
```

Build from the repo root (`external/` is on the include path):

```bash
g++ -std=c++20 example.cpp external/avant-inifile/inifile.cpp external/avant-inifile/value.cpp -Iexternal -o inifile.example.exe && ./inifile.example.exe
```

## Tests

[`test/inifile.test.cpp`](../../test/inifile.test.cpp):

```bash
g++ -std=c++20 test/inifile.test.cpp external/avant-inifile/inifile.cpp external/avant-inifile/value.cpp -o test/inifile.exe && ./test/inifile.exe
```
