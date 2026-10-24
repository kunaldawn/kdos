## About

SQLite is an SQL database engine in a library: a database is one file, opened directly by the
program that uses it, with no server. A great deal of the tree stores its data this way: python3's
`sqlite3` module, util-linux, GnuPG, nss, both Qt bases, WebKitGTK, gdal and the map programs, and
the container stack behind the boxes, where `podman` and `buildah` are built with it. The port
builds upstream's amalgamation tarball, whose library is one large `sqlite3.c`, and turns on at
compile time the optional features listed below.

The `sqlite3` shell that ships beside the library embeds its own copy of `sqlite3.c` rather than
linking `libsqlite3.so`, so the options below reach both.

## Flags

- `-DSQLITE_ENABLE_COLUMN_METADATA=1` — Compile in `sqlite3_column_table_name()`,
  `sqlite3_column_origin_name()`, `sqlite3_table_column_metadata()` and the rest of the column
  metadata interface, which tells a caller which table and column a result column came from.
- `-DSQLITE_ENABLE_UNLOCK_NOTIFY` — Compile in `sqlite3_unlock_notify()`, which lets a connection
  in shared-cache mode wait for a lock held by another connection instead of failing with
  `SQLITE_LOCKED`.
- `-DSQLITE_ENABLE_DBSTAT_VTAB=1` — The `dbstat` virtual table, which reports how the pages of a
  database file are used, table by table.
- `-DSQLITE_ENABLE_FTS3_TOKENIZER=1` — Allow the two-argument form of `fts3_tokenizer()`, through
  which an application registers its own tokenizer for full-text search.
- `-DSQLITE_ENABLE_FTS3_PARENTHESIS` — The enhanced FTS3 query syntax, with `AND`, `NOT` and
  parentheses.
- `-DSQLITE_SECURE_DELETE` — Make `secure_delete` the default: deleted content is overwritten with
  zeros instead of being left in free pages of the file.
- `-DSQLITE_ENABLE_STMTVTAB` — The `sqlite_stmt` virtual table, which lists a connection's prepared
  statements.
- `-DSQLITE_MAX_VARIABLE_NUMBER=250000` — Raise the highest `?NNN` parameter number a statement
  may use, and so the number of values one statement can bind, above upstream's default.
- `-DSQLITE_MAX_EXPR_DEPTH=10000` — Raise the deepest expression tree the parser accepts above
  upstream's default.
- `-DSQLITE_ENABLE_MATH_FUNCTIONS` — The SQL maths functions (`sqrt()`, `log()`, `sin()` and the
  rest). The configure script turns them on by default as well.
- `-DSQLITE_ENABLE_RTREE=1` — The R\*Tree module, a spatial index. A GeoPackage keeps its spatial
  index as an R\*Tree virtual table, so without it a GeoPackage file opens but cannot answer a
  bounding-box query, and gdal's GeoPackage driver requires it.
- `-DSQLITE_ENABLE_GEOPOLY=1` — The Geopoly extension, polygon functions on top of R\*Tree.
- `--enable-fts3` — The FTS3 full-text search module.
- `--enable-fts4` — FTS4, the same module with its additional options.
- `--enable-fts5` — FTS5, the newer full-text search module.
- `--session` — The session extension, which records the changes made to a database as a
  changeset that can be applied elsewhere; it brings the pre-update hook with it.
- `--dbpage` — The `sqlite_dbpage` virtual table, raw access to the pages of the file, which the
  shell's `.recover` command reads.
- `--enable-readline` — Link the shell with [readline](#port-readline) for line editing. Given
  explicitly, a readline that cannot be found stops the configure instead of leaving a shell
  without line editing.
- `--soname=legacy` — Give `libsqlite3.so` the soname `libsqlite3.so.0`, the value upstream's
  configure calls legacy. Without the switch the configure sets no soname at all, and every
  program linked against the library would record the bare `libsqlite3.so` as its dependency.
- `--disable-rpath` — Record no run path in the library or the shell; `/usr/lib` is on the
  loader's default path.
