# avant-sql

MySQL client function library: a connection wrapper, a fixed-size connection
pool, an RAII transaction, and a prepared-statement template.

**Not wired into avant's CMake build** — this directory is a standalone CMake
project (see `CMakeLists.txt`), built separately from the main tree.

```bash
sudo apt-get install libmysqlclient-dev
cmake -S external/avant-sql -B external/avant-sql/build
cmake --build external/avant-sql/build -j4   # links mysqlclient
```

## Components

- `connection` (`include/connection.h`) — owns a `MYSQL*`.
  `connect(ip, user, password, db, port = 3306)` / `close()` / `ping()` /
  `get()` (raw `MYSQL*` for escape hatches).
- `pool` (`include/pool.h`) — fixed-size pool of `shared_ptr<connection>`,
  guarded by a mutex + condition variable. `init(size, ...)` opens `size`
  connections up front (failed ones are skipped, not counted). `get()` blocks
  until a connection is free, `ping`s it, and transparently reconnects a dead
  one (if the reconnect also fails it hands back the stale handle). `back()`
  returns it to the pool.
- `query` (`include/query.h`) — non-prepared `mysql_query` convenience:
  `insert`/`update`/`del` return the affected-rows count; `select` prints
  rows to stdout (debug helper).
- `transaction` (`include/transaction.h`) — RAII: the constructor issues
  `START TRANSACTION`; the destructor rolls back if `commit()` was not called.
  `commit()`/`rollback()` throw on failure. `save_point()` / `rollback_once()`
  manage a LIFO stack of `sp0, sp1, ...` save points for partial rollback.
- `sql<bind_size>` / `result<result_size>` (`include/sql.h`, header templates)
  — prepared-statement wrapper over `MYSQL_STMT` + `MYSQL_BIND`. Map C struct
  fields to columns with `set_bind(index, buffer, size, value_type[, is_null])`
  (params on `sql`, output on `result`), run with `execute(result&)`, then
  `fetch()` rows one at a time — `result.is_null[i]` / `out_length[i]` report
  NULLs and lengths; `get_affected_rows()` for non-SELECT statements.
  `value_type` is an enum over the full `MYSQL_TYPE_*` set.

## Example

```cpp
#include "connection.h"
#include "pool.h"
#include "sql.h"
#include "transaction.h"

using namespace avant::sql;

pool pool;
pool.init(10, "127.0.0.1", "user", "pass", "mydb", 10, 3306);

auto conn = pool.get();
transaction tx(conn);

sql<1> stmt(conn, "SELECT Uin FROM DbMessageData WHERE GID > ? LIMIT 1");
int64_t min_gid = 0;
stmt.set_bind(0, &min_gid, sizeof(min_gid), value_type::LONGLONG);
int32_t uin = 0;
result<1> res;
res.set_bind(0, &uin, sizeof(uin), value_type::LONG);

if (stmt.execute(res) && stmt.fetch())
{
    if (!res.is_null[0])
        std::cout << uin << "\n";
}

tx.commit();
pool.back(conn);
```

A longer tour (pool + transaction + prepared SELECT/UPDATE/DELETE + save
point) lives in `src/main.cpp` — it has the connection info hardcoded, edit it
before running.

## Notes

- Everything is synchronous and blocking; there is no async interface.
- If `init()` fails to open every connection, the pool list stays empty and
  `get()` blocks until something is `back()`ed.
- `sql`/`result` are templates; everything else is compiled in `src/*.cpp`.
