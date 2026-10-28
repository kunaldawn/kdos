## About

The C++ standard library for bare-metal ARM: `libstdc++.a`, `libsupc++.a` and `libstdc++exp.a` for
every multilib of [gcc-arm-none-eabi](#port-gcc-arm-none-eabi), and the headers under
`/usr/arm-none-eabi/include/c++`, built against [picolibc](#port-picolibc-arm-none-eabi). With it
`arm-none-eabi-g++` can compile and link C++ firmware for the Cortex-M and Cortex-R parts of its
`rmprofile` multilib set, and [picotool](#port-picotool), whose RP2350 helper programs are pico-sdk
projects that enable C++, depends on it. It is the second build of the cross compiler's gcc source: libstdc++
compiles against the target C library, and picolibc is built with that compiler, so the compiler,
the C library and this library are three recipes. How the split works is in
[A second build of the same source](../../../../docs/kdos/05-developer/writing-ports.md#a-second-build-of-the-same-source).

## Flags

- `--target=arm-none-eabi`, `--prefix=/usr`, `--libexecdir=/usr/lib`,
  `--with-sysroot=/usr/arm-none-eabi`, `--enable-multilib`, `--with-multilib-list=rmprofile` — The
  options [gcc-arm-none-eabi](#port-gcc-arm-none-eabi) is configured with; `--enable-multilib` is
  gcc's default, which that recipe leaves unstated. They have to match it:
  the prefix, the sysroot and the multilib set decide the directories the libraries are installed
  in, and the installed compiler driver searches only its own.
- `--with-native-system-header-dir=/include` — Where the C library's headers are, relative to the
  sysroot. picolibc's headers are in `/usr/arm-none-eabi/include`, not in the sysroot's
  `/usr/include`, and without the switch the build stops at `fixincludes`, which looks for a
  directory that does not exist.
- `--with-picolibc`, `--without-newlib` — Select libstdc++'s picolibc configuration: the
  `os/picolibc` support files and a fixed list of the functions picolibc provides, taken without
  link tests, which a bare-metal target cannot run. `--with-picolibc` also keeps libtool's `dlopen`
  probe, a link test, out of configure. configure tests for newlib before picolibc, so
  `--with-newlib` would take the newlib branch and its `os/newlib` files instead.
- `--with-headers=no` — In libstdc++'s configure, takes `#include <stdio.h>` out of the prologue
  autoconf puts in front of every compile test, and, with `--without-newlib` beside it, makes the
  library default to a freestanding build, which `--enable-hosted-libstdcxx` reverses.
- `--enable-hosted-libstdcxx` — Builds the hosted library, with streams, strings, containers and the
  rest, which picolibc can support, rather than the freestanding subset.
- `--disable-libstdcxx-verbose` — Leaves the verbose terminate handler out of the library. That
  handler prints the type and message of an uncaught exception, and pulls the demangler and stdio
  into every firmware image that can throw.
- `--disable-shared`, `--disable-threads`, `--disable-libstdcxx-pch` — Static archives only, no
  thread support and no precompiled headers, as in gcc-arm-none-eabi.
- `--disable-libssp`, `--disable-libgomp`, `--disable-libquadmath`, `--disable-libatomic`,
  `--disable-decimal-float` — The other target libraries and features a gcc build can make for its
  target, none of which this build installs.
- `--with-zstd=/usr`, `--without-isl` — The host side of the configured compiler: zstd for
  compressed LTO streams, and no isl loop optimiser, as in gcc-arm-none-eabi. Nothing of the
  compiler itself is installed from this build.
- `--with-pkgversion="KDOS"` — The string the compiler prints in place of `(GCC)` in its version
  line.
- `CFLAGS_FOR_TARGET`, `CXXFLAGS_FOR_TARGET` — The flags for the target libraries: `-Os -pipe
  -ffunction-sections -fdata-sections`, nothing from the host's flags. `-Os` because every byte of
  `libstdc++` a firmware links lands in the part's flash, and the per-function sections let
  `--gc-sections` drop what a firmware image does not call. See
  [A language standard reaching the compiler's own runtime](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-language-standard-reaching-the-compilers-own-runtime).

## Patches

- `picolibc-long-double-math.patch` — In both `libstdc++-v3/configure.ac` and the generated `configure`, adds the `HAVE_*L` definitions (`HAVE_ACOSL` through `HAVE_TANHL`) for the `long double` maths functions to libstdc++'s picolibc branch, which defines only the `float` ones although picolibc has both. Without them `<cmath>` declares no `std::acosl` and its kin, and the C++23 `std` module does not compile.

## Notes

- **The build log is checked for the `std` module.** When the module interface fails, make prints
  `stamp-modules-bits] Error 1 (ignored)` and goes on, and the package would ship a
  `libstdc++.modules.json` that advertises a module that does not exist. `build.sh` tees the output
  of `make all-target-libstdc++-v3` to a log, fails on make's own exit status, and fails again if
  that line is in the log.
- **Only libstdc++ is installed.** The install runs `make install` in `arm-none-eabi/libstdc++-v3`,
  not the top-level `install-target-libstdc++-v3`, which installs `libgcc` first and would give this
  package files gcc-arm-none-eabi owns. The libtool `.la` files are deleted, and so are the
  `-gdb.py` pretty-printer loaders beside each archive and `/usr/share`, where the printers would
  land in `/usr/share/gcc-<version>/python`, the host [gcc](#port-gcc)'s own path; a loader beside a
  static archive is never auto-loaded anyway.
- **The version is the compiler's.** The recipe carries the gcc tarball under gcc-arm-none-eabi's
  file name and checksum, and both recipes carry `group = gcc-arm-none-eabi`, so the two are bumped
  together.
