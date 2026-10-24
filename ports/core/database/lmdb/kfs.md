## About
LMDB, the Lightning Memory-Mapped Database, is an embedded key-value store from the OpenLDAP
project: a copy-on-write B+tree in one memory-mapped file, with ACID transactions, many readers
alongside one writer, and reads that copy nothing. [nheko](#port-nheko) keeps its Matrix state in
it, and [baloo](#port-baloo), KDE's file indexer, its index; [lmdbxx](#port-lmdbxx) is the C++
wrapper nheko uses. The port installs `liblmdb.so`, `lmdb.h`, the `mdb_stat`, `mdb_copy`,
`mdb_dump` and `mdb_load` tools with their manual pages, and a pkg-config file.

## Flags
- `CC="$CC"` — The tree's compiler; the Makefile names `gcc`.
- `OPT="-O2"` — Replace the Makefile's `OPT`, `-O2 -g`, so the library is built without debug
  information.
- `XCFLAGS="$CFLAGS"` — Add the tree's `CFLAGS`: the Makefile builds its `CFLAGS` from `THREADS`,
  `OPT`, its warning flags and `XCFLAGS`, so a plain `CFLAGS` given to make would replace the
  `-pthread` and warnings too.
- `LDFLAGS="$LDFLAGS"` — The tree's link flags, on every link. The Makefile sets no `LDFLAGS` of
  its own, so this names on the command line what make would otherwise take from the
  environment.
- `prefix=/usr` — The Makefile's install prefix, `/usr/local` by default.

## Patches
- `lmdb-make.patch` — Makes the Makefile build a versioned shared library: `liblmdb.so.0.0.0` with the soname `liblmdb.so.0`, and the symlinks `liblmdb.so.0` and `liblmdb.so` to it. Upstream builds `liblmdb.so` with no soname and a static `liblmdb.a`, and links its tools against the static one; with the patch no static library is installed, the tools link the shared library, and `make install` copies with `cp -a` so the symlinks stay symlinks.

## Notes
The library is in `libraries/liblmdb` of an OpenLDAP source archive cut at the `LMDB_<version>`
tag, so `build.sh` works from that directory. Upstream ships no pkg-config file; `build.sh` writes
`lmdb.pc` with the recipe's version, for consumers that find LMDB through `pkg-config lmdb`.
