## About
Berkeley DB is the embedded key-value database library: B-tree, hash, queue and record-number
access methods over files, with transactions, locking and recovery. The port builds the C library
`libdb`, the C++ binding `libdb_cxx`, the `db_185.h` compatibility interface and the `db_*`
utilities (`db_dump`, `db_load`, `db_verify`, `db_recover` and the rest). Among the ports built
on it are [iproute2](#port-iproute2), [perl-db-file](#port-perl-db-file) (Perl's `DB_File`
module) and [xastir](#port-xastir).

## Flags
- `-fpermissive` — Added to `CFLAGS` so that configure's test programs, written with implicit
  function declarations and implicit `int`, still compile: the compiler rejects both by default,
  and a test that fails to compile reads to configure as a missing feature.
- `--enable-compat185` — Build the DB 1.85 compatibility interface, `dbopen()` and `<db_185.h>`,
  for programs written to the original BSD `db` API.
- `--enable-cxx` — Build the C++ API, `libdb_cxx` and `<db_cxx.h>`.

## Patches
- `atomic.patch` — Renames Berkeley DB's internal `__atomic_compare_exchange()` in `src/dbinc/atomic.h` to `__atomic_compare_exchange_db()`. GCC has a built-in function of that name, and a static inline function that redefines it does not compile.

## Notes
configure is run from `build_unix`, the directory Berkeley DB's build instructions use, as
`../dist/configure`.

`make install` would also run `install_docs`, which copies Berkeley DB's whole HTML documentation
set into the package. `build.sh` names the other three install targets instead,
`install_include`, `install_lib` and `install_utilities`, installs the licence by hand, and checks
that `db.h` and `libdb.so` are in the package.
