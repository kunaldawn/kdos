## About

ccache is a compiler cache: it hashes a compile's source, its options and the compiler,
and when it has seen the same compile before it hands back the stored object instead of running the
compiler. For KDOS it is a build tool first. Once `30_foundation` has installed it,
`script/env/chroot.env` sends every CMake project of every later chroot step through it, and the
cache in `build/ccache` survives `make cleanbuild`. The settings that keep a cached object
byte-identical to a fresh compile are in
[The compiler cache](../../../../docs/kdos/05-developer/how-kdos-is-built.md#the-compiler-cache).

The package also ships the masquerade directory `/usr/lib/ccache`, whose `gcc`, `g++`, `cc`, `c++`,
`clang` and `clang++` are symlinks to ccache: a user who puts that directory first on `PATH` gets
every compile cached. The build itself never puts it on `PATH`, because CMake would record
`/usr/lib/ccache/gcc` as the compiler in the files a package ships. The manual page is a separate
port, [ccache-manual](#port-ccache-manual), built once asciidoctor is installed.

## Flags

- `-DCMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG"` — Replace CMake's Release flags, `-O3 -DNDEBUG`,
  with the tree's own flags and `-DNDEBUG`. ccache's C code builds at the tree's `-O2`, with its
  assertions compiled out.
- `-DCMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS -DNDEBUG"` — The same for the C++ code, which is most of
  ccache.
- `-DCMAKE_INSTALL_LIBEXECDIR=lib` — Point CMake's helper-program directory at `/usr/lib` instead
  of `/usr/libexec`. ccache installs nothing there, but compiles the full path in as the default
  of its `libexec_dirs` setting, the directories it searches for a remote-storage helper program,
  `ccache-storage-<scheme>`, before its own directory and `PATH`.
- `-DDEPS=LOCAL` — Use only libraries found on the system or bundled in the source. Under the
  default, `AUTO`, a required library missing from the system (xxhash, zstd) is downloaded, and the
  chroot has no network; `LOCAL` makes a missing one stop the configure instead. zstd and xxhash
  come from the ports in `depends`.
- `-DDEP_FMT=BUNDLED` — Build against the copy of fmt in `src/third_party` even when a system fmt
  is installed. fmt is a [port](#port-fmt) of a later phase, so without this a build in
  `30_foundation` and a rebuild after `41_system` would link different libraries.
- `-DDEP_BLAKE3=BUNDLED`, `-DDEP_CPPHTTPLIB=BUNDLED`, `-DDEP_TLEXPECTED=BUNDLED` — The same pin for
  BLAKE3, cpp-httplib and tl-expected, which the tree carries no port of, so a library installed
  later under the same name never changes what ccache links.
- `-DCCACHE_DEV_MODE=OFF` — Dev mode is on whenever a `CI` variable is in the environment or the
  source came from git. It adds `-Werror` to every compile and makes the build probe for a faster
  linker (lld or gold) and link with whichever it finds, so the result would follow what the chroot
  happens to hold.
- `-DHTTP_STORAGE_BACKEND=ON` — Build the HTTP remote-storage backend, which lets a cache be shared
  through a web server. It is upstream's default; it is what needs cpp-httplib.
- `-DREDIS_STORAGE_BACKEND=OFF` — Build no Redis remote-storage backend. It needs hiredis, which
  the tree does not carry, and with `DEPS=LOCAL` a missing hiredis would stop the configure.
- `-DENABLE_TESTING=OFF` — Build no unit tests; they need doctest, which the tree does not carry.
- `-DENABLE_DOCUMENTATION=OFF` — Generate no manual. The generator is asciidoctor, a Ruby program
  that is installed only in a later phase; `ccache-manual` builds the page from the same source.

## Notes

The masquerade directory names `clang` and `clang++` although neither compiler is installed in
`30_foundation`. A link to a compiler that is not installed does nothing until it is; a compiler
with no link in the directory is one that a compile through it runs uncached, with no warning.
