## About

Zstandard is a compression format whose levels run from very fast to very tight: the library
`libzstd`, the `zstd`, `unzstd`, `zstdcat` and `zstdmt` commands, and the `zstdgrep` and
`zstdless` scripts. KDOS uses it in several of its own mechanisms:

- The live image's `system.sfs` is compressed with zstd by `mksquashfs`, unless `KDOS_ISO_COMP`
  selects xz (see
  [The live medium and persistence](../../../../docs/kdos/03-architecture/boot-and-init.md#the-live-medium-and-persistence)).
- A pack is an EROFS image compressed with zstd, and a pack delta is `zstd --patch-from` between
  two packs (see [Deltas](../../../../docs/kdos/03-architecture/packs-and-boxes.md#deltas)).
- `kpkg delta` makes a package delta with `zstd --patch-from` over the uncompressed archives (see
  [Deltas](../../../../docs/kdos/03-architecture/packaging.md#deltas)).

It builds in `30_foundation`, after [zlib](#port-zlib), [xz](#port-xz) and [lz4](#port-lz4), whose
formats its command also reads and writes. It does not exist in `20_selfhost`, which is why
[binutils](#port-binutils) and [gcc](#port-gcc) are built `--without-zstd`.

The build is upstream's plain makefiles, with no configure step; every setting is a make variable,
collected in the `zstd_vars` array and passed to both `make` lines.

## Flags

- `${CFLAGS/-O2/-O3}` — raise the exported `CFLAGS` and `CXXFLAGS` to `-O3`. zstd's makefiles
  default to `-O3` only when `CFLAGS` is unset, and the tree always sets it. This is the "`-O3` for
  a hot port by pattern" case of
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port).
- `-flto=auto` — compile and link with link-time optimisation, added to `LDFLAGS` as well. zstd is
  one of the hot libraries the tree builds with LTO (see
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port), "LTO only for the
  hot libraries").
- `-ffat-lto-objects` — keep ordinary machine code beside the LTO bytecode, so the shipped
  `libzstd.a` links with any linker, not only through GCC's plugin.
- `-frandom-seed=zstd` — fix the seed GCC names each object's `.gnu.lto_*` sections with, so
  `libzstd.a` is the same bytes on every build.
- `ZSTD_LEGACY_SUPPORT=5` — keep decoders for the pre-1.0 legacy formats from v0.5.0 on, so
  `zstd -d` and every program decompressing through the library still read such frames. This is
  the makefile's default, named because `ZSTD_LEGACY_FILES` must agree with it.
- `ZSTD_LEGACY_FILES` — the legacy decoder sources to compile, `lib/legacy/zstd_v05.c` to
  `zstd_v07.c`, given as full paths. The makefile otherwise collects them by running `ls` and
  `grep` over `lib/legacy`. The list must match `ZSTD_LEGACY_SUPPORT`: the library is compiled with
  `-DZSTD_LEGACY_SUPPORT=5` and calls the decoders of every version from there on.
- `HAVE_PTHREAD=1` — build the multithreaded compressor, `zstd -T`, without the makefile's probe
  compile.
- `HAVE_ZLIB=1`, `HAVE_LZMA=1`, `HAVE_LZ4=1` — build the `--format=gzip`, `--format=xz`,
  `--format=lzma` and `--format=lz4` support of the `zstd` command. Each skips the makefile's probe, which compiles a test
  program and quietly drops the format when it fails. Set outright, a missing zlib, xz or lz4 is a
  link error instead of a `zstd` that reads fewer formats.
