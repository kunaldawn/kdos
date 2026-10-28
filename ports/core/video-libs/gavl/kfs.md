## About

gavl is the Gmerlin audio and video library: conversion between pixel formats, colourspaces and
sample formats, and scaling. In KDOS it is there for [frei0r-plugins](#port-frei0r-plugins), whose plugins that
scale or convert frames are built with it. Only the library, its headers and its pkg-config file
are installed.

## Flags

- `--without-doxygen` — No Doxygen API reference. Without the switch configure looks for
  `doxygen` and stops with an error when it is not installed.
- `--with-cpuflags=none` — Add no processor-specific compiler flags. The default, `auto`, runs
  `cpuinfo.sh` over `/proc/cpuinfo` and compiles for the processor of the machine that builds,
  so the library would fault on an older one. The SIMD paths are chosen at run time either way.
- `LDFLAGS="$LDFLAGS"` — On the first `make -C gavl` line. configure replaces `LDFLAGS` with its own value, dropping the
  tree's link flags, so they are given again on the `make` line, where they override it. The link
  otherwise loses `-z now` and the package lint warns (see
  [What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).

## Patches

- `no-x87-fpmath.patch` — Edits the generated `configure` so that it does not add `-mfpmath=387` to `CFLAGS` when the compiler accepts the flag, as GCC does on x86-64. With the flag, the library's floating-point arithmetic is compiled for the x87 unit instead of SSE, the x86-64 default.

## Notes

`build.sh` builds and installs two subdirectories, `gavl` and `include`, and then installs
`gavl.pc` from the top, instead of running `make` over the whole tree. The other directories hold
the test and benchmark programs (`src`) and the utilities (`utils`), which a plain `make` compiles
with the library. Several of the tests, `scaletest.c` and `blend_test.c` among them, call
`memset` without including `<string.h>`, an implicit declaration that GCC 14 and later reject as an
error.
