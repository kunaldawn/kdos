## About

elfutils is a set of libraries and tools for ELF object files and DWARF debug information.
[libelf]{.idx} reads and writes ELF files, [libdw]{.idx} reads DWARF and unwinds stacks, and
`libasm` assembles and disassembles; the tools (`eu-readelf`, `eu-strip`, `eu-stack`,
`eu-addr2line` and the rest) are built on them.

Most of what KDOS uses elfutils for is the libraries. The kernel build (`linux`) and `perf`,
`libbpf`, `bpftool`, `bpftrace`, `bcc` and `dwarves` read ELF files through libelf; `strace`
prints stack traces with libdw (`--with-libdw`); `glib` is built with `-D libelf=enabled`; and
`mesa`, `qemu`, `gstreamer`, `ltrace`, `iproute2` and others name it in `depends`. It is in
`30_foundation` with the musl compatibility libraries it is built against, argp-standalone,
musl-fts, musl-obstack and musl-rpmatch, listed just above it.

## Flags

- `-Wno-error` — elfutils compiles its sources with `-Werror` (`AM_CFLAGS` and `AM_CXXFLAGS` in
  `config/eu.am`). automake puts `CFLAGS` after the project's own flags, so for the C sources this
  later switch turns warnings back into warnings, and a new warning from a newer compiler or C
  library header does not stop the build. It is not in `CXXFLAGS`, so the one C++ source this
  build compiles, `eu-srcfiles`' `src/srcfiles.cxx`, still builds with `-Werror`.
- `-include libgen.h` — forces `<libgen.h>`, which declares musl's `basename()`, into every
  compile ahead of the source's own includes. elfutils' `lib/system.h` defines its own
  `xbasename()` and then poisons the bare name with `#pragma GCC poison basename`, which forbids
  any later use; a declaration seen before the pragma is not an error.
- `-include rpmatch.h`, `-lrpmatch` (in `LDFLAGS`) — force musl-rpmatch's header into every
  compile and add its library to every link. The elfutils source in this version calls
  `rpmatch()` nowhere, so neither has an effect: the header adds an unused declaration, and the
  tree's `-Wl,--as-needed` drops the library from every link, so no installed file needs
  `librpmatch`.
- `-DFNM_EXTMATCH=0` — musl's `<fnmatch.h>` has no `FNM_EXTMATCH`, the glibc flag that lets a
  pattern use ksh-style groups such as `@(a|b)`. elfutils passes it in `eu-strip`'s
  `--remove-section` and `--keep-section` patterns and `eu-elfcompress`'s `--name`; defined as 0,
  those patterns are plain shell globs, although `--help` still calls them "extended wildcard"
  patterns. One default is lost with it: `eu-elfcompress` given no `--name` matches sections
  against `.?(z)debug*`, which as a plain glob matches no real section name, so it reports
  "Nothing to do." Name the sections, as in `eu-elfcompress --name='.debug*' file.o`.
- `--program-prefix="eu-"` — installs every tool as `eu-<name>`, so `eu-strip`, `eu-ar` and
  `eu-readelf` sit beside binutils' `strip`, `ar` and `readelf` instead of overwriting them.
  `eu-` is upstream's default when no prefix is given; the recipe states it.
- `--disable-debuginfod`, `--disable-libdebuginfod` — build neither the debuginfod server nor the
  client library. debuginfod serves debug information and sources over HTTP, and the client lets
  libdw and the `eu-` tools fetch what a binary lacks from such a server. With the client off,
  `libdw` makes no network request when it looks for missing debug files, and the package needs
  neither libcurl nor json-c.
- `--disable-stackprof` — do not build `eu-stackprof`, a sampling profiler built on
  `perf_event_open`. Without the switch, `configure` builds it whenever it finds `json-c`, a
  C++20 `<format>` and a kernel header with build-id support in `perf_event_attr`, so the package
  would gain a program depending on what happened to be installed in the chroot.
- `--with-zlib`, `--with-bzlib`, `--with-lzma`, `--with-zstd` — let libdwfl read ELF and debug
  files compressed with gzip, bzip2, xz and zstd. Each is otherwise used only if found; named,
  `configure` stops when its library is missing, so the package cannot lose a format silently.
  libelf also uses zstd and zlib for compressed ELF sections.
- `--with-libarchive` — link `eu-srcfiles` against libarchive, which gives it `--zip`: collect
  every source file a binary's DWARF names into one zip archive on standard output. Without
  libarchive the option is not compiled in.

## Notes

argp, fts and obstack need no switch: `configure` searches for `argp_parse`, `fts_close` and
`_obstack_free` in the C library first and then in `-largp`, `-lfts` and `-lobstack`, and stops if
none has them. That is why argp-standalone, musl-fts and musl-obstack are build dependencies;
musl-rpmatch is one only because `build.sh` names its header and library. In the built
package `libdw.so` needs `libfts.so.0`; `eu-ar`, `eu-ranlib` and `eu-nm` need `libobstack.so.1`;
and the argp code is copied into each program from the static `libargp.a`.
