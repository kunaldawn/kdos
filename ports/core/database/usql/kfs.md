## About
usql is a universal command-line SQL client modelled on PostgreSQL's `psql`: the same backslash
commands, variables and completion, against many kinds of database through Go's `database/sql`
drivers, with connections named as URLs (`pg://`, `sq:file.db`, `my://`, `dk:file.duckdb`). It can
copy query results between databases with `\copy`.

## Flags
- `CGO_ENABLED=1` — Build with cgo, which the SQLite and DuckDB drivers need: both bind a C
  library.
- `-tags "no_base postgres sqlite3 libsqlite3 mysql csvq duckdb duckdb_use_lib"` — Choose the
  drivers. `no_base` drops upstream's base set (PostgreSQL, MySQL, SQLite, Microsoft SQL Server,
  Oracle and CSVQ), and the tags after it add back PostgreSQL, SQLite, MySQL, CSVQ (CSV, TSV and
  JSON files queried as tables) and DuckDB. `libsqlite3` makes `go-sqlite3` link the system
  [sqlite](#port-sqlite) instead of compiling the SQLite amalgamation it carries, and
  `duckdb_use_lib` makes DuckDB's Go bindings link the system `libduckdb` from
  [duckdb](#port-duckdb) instead of the prebuilt static archives the bindings carry in
  `lib/linux-amd64`.

## Notes
usql is built from a vendor bundle, `usql-vendor-<version>.tar.xz`, made by `ports/fetch` with
`go mod vendor`; see [Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring). The
installed binary NEEDs `libsqlite3.so.0` and `libduckdb.so`.
