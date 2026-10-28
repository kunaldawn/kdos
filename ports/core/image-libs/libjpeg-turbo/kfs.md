## About

libjpeg-turbo is the [JPEG]{.idx} codec: `libjpeg`, the IJG API that nearly every program reading or
writing a JPEG links, accelerated with SIMD kernels, and `libturbojpeg`, its own simpler API. It
also installs `cjpeg`, `djpeg`, `jpegtran`, `rdjpgcom`, `wrjpgcom` and `tjbench`. It is one of the
most depended-on libraries in the tree: libtiff, libwebp, libjxl, libheif, libavif, gdk-pixbuf, the
toolkits and the browsers all link it.

## Flags

- `-DWITH_JPEG8=ON` — Emulate the libjpeg v8 API and ABI. The library is installed as
  `libjpeg.so.8`, and is not binary-compatible with the libjpeg v6b ABI (`libjpeg.so.62`) that is
  upstream's default. Every program in the tree is linked against the v8 name, so changing this
  switch changes the soname and needs every dependent rebuilt.
- `-DWITH_SIMD=ON` — Build the SIMD extensions, which `nasm` assembles for x86-64. This is
  upstream's default, named in the recipe.
- `-DREQUIRE_SIMD=ON` — Stop the configure if the SIMD extensions cannot be built. Without it a
  missing or unusable assembler is only a warning, and the library ships with the plain C code paths
  alone; `nasm` is in `depends` for this reason.
- `-DWITH_SYSTEM_SPNG=ON` — Read and write PNG in `cjpeg`, `djpeg`, `tj3LoadImage()` and
  `tj3SaveImage()` through the [libspng](#port-libspng) port, instead of the copy of libspng in the
  source tree.
- `-DWITH_SYSTEM_ZLIB=ON` — Link the system zlib. The option exists only when the bundled libspng is
  used; with `-DWITH_SYSTEM_SPNG=ON` upstream already uses the system zlib, so this switch only
  states it.
- `-DCMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG"` — Replace CMake's Release flags, `-O3 -DNDEBUG`,
  with the tree's `CFLAGS` and `-DNDEBUG`. CMake puts the Release flags after `CFLAGS` on every
  compile, so its `-O3` would otherwise override the tree's `-O2`.
- `-DCMAKE_SKIP_INSTALL_RPATH=ON` — Install the libraries and programs with no run path. Whenever
  shared libraries are built, upstream sets `CMAKE_INSTALL_RPATH` to the library directory, which
  here is `/usr/lib`, already the default search path, so the entry adds nothing and the package
  lint warns on it (see
  [What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
- `-DWITH_TESTS=OFF` — Build none of the regression-test programs; this is the project's own name
  for the choice `-DBUILD_TESTING=OFF` makes elsewhere.

## Patches

- `config-spng-dependency.patch` — makes the installed `libjpeg-turboConfig.cmake` create the `PkgConfig::spng` target, through `find_dependency(PkgConfig)` and `pkg_check_modules`, when the library was built with `WITH_SYSTEM_SPNG`. The exported TurboJPEG targets name `PkgConfig::spng` in their link interface and upstream's file never defines it, so a CMake consumer that links them stops with the target not found.

## Notes

The recipe leaves `ENABLE_STATIC` at its default, so `libjpeg.a` and `libturbojpeg.a` are installed
beside the shared libraries, with the CMake package files under `/usr/lib/cmake/libjpeg-turbo`.
libtiff finds the library through that CMake package, and falls back to the `FindJPEG` module in
its own `cmake/` directory only when the package is missing.
