## About
CMake generates the build files of a large share of the tree's C and C++ ports, and `cpack`,
`ctest` and `ccmake`, its curses interface, come with it. It is built in the opening run of
`30_foundation`, where no CMake exists yet, so it is configured with its own `bootstrap` script:
the script compiles a minimal CMake from the source with the C and C++ compilers alone, and that
CMake then configures and builds the full one with Make. How a CMake port is written in this tree
is in
[Writing ports](../../../../docs/kdos/05-developer/writing-ports.md#cmake).

The port's dependencies are built ahead of it in the same run, Sphinx and its Python modules among
them (see [Part VI](../../../../docs/kfs/06-foundation/intro.md)).

## Flags
- `--no-backup-if-mismatch` — Keep `patch` from leaving a `GNUInstallDirs.cmake.orig` beside the
  patched module if the hunk applies at an offset or with fuzz. The install copies the whole `Modules` directory,
  so a `.orig` would be shipped, and the package lint refuses one (see
  [What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
- `-include cstdint` — Include `<cstdint>` ahead of every C++ source file, which declares the
  fixed-width integer types in each file whether or not it includes the header itself. GCC's C++
  library headers do not bring `<cstdint>` in on the way to their own, so a source file that uses
  `uint64_t` and the other fixed-width types without including it does not compile (see
  [A standard header the source never includes](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-standard-header-the-source-never-includes)).
- `--datadir` — Install the modules, templates and help under `/usr/share/cmake`. The bootstrap
  takes these directories relative to the prefix, so each value starts with `/` and names no `usr`.
  Upstream's default, `share/cmake-<major>.<minor>`, changes its name with each minor release.
- `--docdir` — Install the documentation under `/usr/share/doc/cmake`. The bootstrap's default,
  `doc/cmake-<major>.<minor>`, puts it in `/usr/doc`, a path the package lint refuses (see
  [The package lint](../../../../docs/kdos/03-architecture/packaging.md#the-package-lint)).
- `--mandir` — Install the manual pages under `/usr/share/man`, where every port puts its pages.
  The bootstrap's default is `man`, which would put them in `/usr/man`, outside the manual roots the
  image's index covers (see
  [Databases stamped into the image](../../../../docs/kdos/05-developer/build-system.md#databases-stamped-into-the-image)).
- `--sphinx-man` — Build the manual pages from CMake's reStructuredText documentation with Sphinx.
  The source ships no formatted pages, so without it the package has none; this is why the recipe
  depends on [python3-sphinx](#port-python3-sphinx) (see
  [Manual pages](../../../../docs/kdos/05-developer/writing-ports.md#manual-pages)).
- `--sphinx-build` — The `sphinx-build` to run, `/usr/bin/sphinx-build`.
- `--system-libs` — Link the system's copy of every third-party library CMake can use instead of
  the copy bundled in its source: zlib, bzip2, liblzma, zstd, nghttp2, libarchive, libuv, Expat,
  ncurses' libform (for `ccmake`), curl, jsoncpp, cppdap and librhash. The four `--no-system-` switches before it take the last four
  back: CMake applies `--system-libs` only to a library no switch of its own has decided, whatever
  the order on the command line.
- `--no-system-jsoncpp` — Use the bundled jsoncpp. [jsoncpp](#port-jsoncpp) is a port of
  `41_system`, a later phase.
- `--no-system-cppdap` — Use the bundled cppdap, the Debug Adapter Protocol library behind
  `cmake --debugger`. It is not a port in the tree.
- `--no-system-librhash` — Use the bundled librhash, the hash library behind CMake's `file(HASH)`
  and `string(HASH)` commands. It is not a port in the tree.
- `--no-system-curl` — Use the bundled curl. [curl](#port-curl) cannot be a dependency of CMake:
  it depends on [brotli](#port-brotli), which is built with CMake.
- `CMake_BUILD_LTO=ON` — Build CMake itself with link-time optimisation, through CMake's own
  option for it, when the compiler supports it.
- `BUILD_CursesDialog=ON` — Build `ccmake`. Left unset, CMake decides by test-compiling a curses
  program and quietly leaves `ccmake` out when that fails.
- `CMAKE_REQUIRE_FIND_PACKAGE_Curses=ON` — Make CMake's `find_package(Curses)` required, so a
  missing ncurses stops the configure. With `BUILD_CursesDialog=ON` alone, a missing ncurses only
  warns and turns `ccmake` off.

## Patches
- `gnuinstalldirs-lib.patch` — Deletes the branch of `Modules/GNUInstallDirs.cmake` that makes `CMAKE_INSTALL_LIBDIR` default to `lib64` on a 64-bit Linux system it does not recognise as Alpine, Arch, Debian or a conda environment, by the files `/etc/alpine-release`, `/etc/arch-release` and `/etc/debian_version`. KDOS has none of them, and with the branch gone the default is `lib`. `/usr/lib64` is only a link to `/usr/lib` on KDOS (see [Syncing `fs/`](../../../../docs/kdos/05-developer/build-system.md#syncing-fs)), and the package lint refuses a package with a `usr/lib64` path (see [The package lint](../../../../docs/kdos/03-architecture/packaging.md#the-package-lint)). The patch changes every CMake port built after it, not CMake alone.

## Notes
CMake's configure output names what it bundles, as `Using bundled: CPPDAP CURL JSONCPP LIBRHASH
KWIML`. KWIML, Kitware's header-only library of ABI and integer macros, is taken from the system
only when it is asked for by name, which `--system-libs` does not do. Every other third-party library
comes from the system, and one that `--system-libs` asks for and does not find stops the configure
with an error rather than falling back to the bundled copy.
