## About

LZ4 is a compression format built for speed rather than ratio: the library `liblz4` and the `lz4`,
`unlz4` and `lz4cat` commands. In KDOS it is a dependency of [zstd](#port-zstd), whose command can
read and write `.lz4`, and of the filesystem and archive tools, [erofs-utils](#port-erofs-utils),
[f2fs-tools](#port-f2fs-tools) and [libarchive](#port-libarchive) among them. It is one of the
ports `kdos march` measures with a benchmark of its own (see
[kdos march](../../../../docs/kdos/04-programs/kdos-command.md#kdos-march)); the `bench` and
`bench_setup` lines of the recipe are that benchmark.

The build is upstream's plain makefiles, with no configure step: the exported flags reach them
through the environment, and `PREFIX` and `DESTDIR` place the install.

## Flags

- `${CFLAGS/-O2/-O3}` — raise the exported `CFLAGS` and `CXXFLAGS` to `-O3`. The library's
  and the programs' makefiles put their own `-O3` first and the environment's `CFLAGS` after it,
  so the tree's `-O2` would otherwise win. This is the "`-O3` for a hot port by pattern" case of
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port).
- `-flto=auto` — compile and link with link-time optimisation, running the link-time stage in
  parallel through make's job server, or on every processor when there is none. lz4 is one of the
  short list of hot libraries the tree builds with LTO (see
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port), "LTO only for the
  hot libraries"). It is added to `LDFLAGS` too, so the link step optimises.
- `-ffat-lto-objects` — write ordinary machine code into every object beside the LTO bytecode.
  The package ships `liblz4.a`, and without fat objects that archive would hold bytecode only and
  link only through GCC's plugin.
- `-frandom-seed=lz4` — fix the seed GCC uses to name each object's `.gnu.lto_*` sections. The
  static archive keeps those sections, and with a random seed it would differ on every build, so
  the package would not be reproducible.
