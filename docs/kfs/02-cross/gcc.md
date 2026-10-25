# Cross GCC, pass 1

The second step compiles GCC to run in the container and write programs for
`x86_64-kdos-linux-musl`. It is a deliberately incomplete compiler. There is no C library for the
target yet, so it is built with no libc headers, no shared libraries, no threads and no C++
standard library: enough to compile [musl]{.idx}, and then, with musl in place, everything else
`10_bootstrap` builds. This page shows the commands of `script/phases/00_cross/01_gcc.sh`, explains
each switch, the `limits.h` fix-up that follows the install, and what the compiler leaves in
`build/cross`.

**Source:** the archives of the [gcc](#port-gcc), [gmp](#port-gmp), [mpfr](#port-mpfr) and
[mpc](#port-mpc) ports\
**Installs into:** `build/cross`\
**Approximate build time:** 7.8 SBU

## Installation of cross GCC

GCC needs three mathematical libraries, GMP, MPFR and MPC, to fold constants at compile time. The
step unpacks all four sources and links the three libraries into the GCC tree under the names
GCC's build looks for:

<!-- kfs:include script/phases/00_cross/01_gcc.sh lines=24-34 pin=6d3ecb33bc32 -->

With a `gmp`, `mpfr` and `mpc` directory at the top of its source, GCC builds the three as part of
itself and links them statically into its compiler proper. The cross compiler therefore depends on
none of the container's libraries for them.

Configure and build in a separate directory:

<!-- kfs:include script/phases/00_cross/01_gcc.sh lines=36-61 pin=3f43c2b33782 -->

### The meaning of the configure options

`--target=$KDOS_TARGET`, `--with-sysroot=$SYSROOT`, `--prefix=$CROSS_SYSROOT`
: As for [binutils](binutils.md#the-meaning-of-the-configure-options): a compiler for
  `x86_64-kdos-linux-musl`, whose system root is `build/fs` and which is installed under
  `build/cross`. With the system root set, the compiler searches `build/fs/usr/include` and
  `build/fs/usr/lib` and never the container's directories.

`--without-headers`, `--with-newlib`
: Build with no C library headers. Together the two define `inhibit_libc` while `libgcc` is
  compiled, so the parts of it that need a C library are left out. Nothing of newlib is used:
  `--with-newlib` is the conventional way to say that the target's C library is not available.

`--enable-languages=c,c++`
: Build the C and C++ front ends. C is what musl is written in. The C++ front end compiles
  [libstdc++](../03-bootstrap/030-libstdcxx.md) once musl exists, and after it every C++ program of
  the bootstrap, GCC's own native build among them.

`--enable-default-pie`, `--enable-default-ssp`
: Compile position-independent executables and use the stack protector unless told otherwise.
  Every program of the bootstrap is compiled with this compiler, so every one of them is hardened
  the way the packages that replace them are. musl provides the stack protector's failure
  handler, so no separate `libssp` is needed.

`--enable-initfini-array`
: Run constructors and destructors through `.init_array` and `.fini_array`. A cross build cannot
  test the target for this, but GCC's `config.gcc` presumes it for every Linux target except
  uClibc, so on this [triplet]{.idx term="target triplet"} the switch states the default.

`--disable-nls`
: Build no message catalogues.

`--disable-shared`
: Build `libgcc` as a static archive only. A shared `libgcc_s` would have to be linked against a C
  library.

`--disable-multilib`
: Build for 64-bit x86 alone. KDOS has no 32-bit C library to pair a 32-bit `libgcc` with.

`--disable-threads`
: There is no thread library to build against, so `libgcc` is built for a single thread.

`--disable-libatomic`, `--disable-libgomp`, `--disable-libquadmath`, `--disable-libssp`, `--disable-libvtv`
: Leave out the runtime libraries that need a C library to build. None of them is wanted from a
  compiler whose only task is the bootstrap.

`--disable-libstdcxx`
: Leave out the C++ standard library. It needs a C library, and
  [step 030](../03-bootstrap/030-libstdcxx.md) builds it from the same source once musl is in
  `build/fs`.

`--disable-bootstrap`
: Build the compiler once, with the container's compiler, instead of GCC's three-stage build that
  compiles itself and compares the results. A cross compiler's output cannot run in the container,
  so the comparison is not possible; the switch states it.

`BUILD_INFO=no-info`
: Given to both `make` lines: build and install no Texinfo manuals. Nobody reads the cross
  compiler's manuals, and the native compiler's are installed by its port.

## The limits.h fix-up

<!-- kfs:include script/phases/00_cross/01_gcc.sh lines=63-67 pin=748b4e376267 -->

GCC installs a `limits.h` of its own in its private include directory, and normally that header
ends by including the system's `limits.h` with `#include_next`. GCC decides which version to
install by looking for the system header while it builds. Here it found none, because musl's
headers are not in `build/fs` yet, so it installed a self-contained header that never includes the
system's. Every program compiled afterwards would then miss what musl's `limits.h` defines, such as
`PATH_MAX`.

The `cat` writes the full header the way GCC's build does when the system header exists: the
`limitx.h` prologue, GCC's own `glimits.h`, and the `limity.h` epilogue, into the include
directory beside `libgcc.a`, which `-print-libgcc-file-name` locates. The `#include_next` finds
musl's `limits.h` from step 020 onwards.

The link then gives the compiler the name `x86_64-kdos-linux-musl-cc`. A configure script given
`--host` looks for the target's `gcc` first and its `cc` after it; with the link, either name finds
this compiler. `ln -sv` has no `-f`, so the step stops here when it is run again over a
`build/cross` that already has the link.

## Contents of cross GCC

**Installed programs**, in `build/cross/bin`, each prefixed `x86_64-kdos-linux-musl-`: [`c++`]{.idx},
[`cc`]{.idx} (the link), [`cpp`]{.idx}, [`g++`]{.idx}, [`gcc`]{.idx}, `gcc-` followed by the version, [`gcc-ar`]{.idx}, [`gcc-nm`]{.idx},
[`gcc-ranlib`]{.idx}, [`gcov`]{.idx}, [`gcov-dump`]{.idx}, [`gcov-tool`]{.idx} and [`lto-dump`]{.idx}.

**Installed libraries**, in `build/cross/lib/gcc/x86_64-kdos-linux-musl/<version>`: [`libgcc.a`]{.idx},
[`libgcov.a`]{.idx}, and the start and end files `crtbegin*.o`, `crtend*.o`, [`crtfastmath.o`]{.idx} and
`crtprec*.o`. `build/cross/lib` holds [`libcc1`]{.idx}, the GDB plugin library.

**Installed directories:** the compiler proper ([`cc1`]{.idx}, [`cc1plus`]{.idx}, [`collect2`]{.idx}, [`lto1`]{.idx},
[`lto-wrapper`]{.idx} and the LTO plugin) in `build/cross/libexec/gcc/x86_64-kdos-linux-musl/<version>`,
and GCC's private headers, [`limits.h`]{.idx} among them, in the `include` directory beside `libgcc.a`.
Step 030 adds the C++ headers in `build/cross/x86_64-kdos-linux-musl/include/c++/<version>`.

`gcc`, `cc`
: The C compiler driver: runs the preprocessor, the compiler proper, the assembler and the linker.

`g++`, `c++`
: The C++ compiler driver.

`cpp`
: The C preprocessor.

`gcc-ar`, `gcc-nm`, `gcc-ranlib`
: Wrappers that hand [`ar`]{.idx}, [`nm`]{.idx} and [`ranlib`]{.idx} the LTO plugin, so archives of LTO objects get a
  symbol index.

`gcov`, `gcov-dump`, `gcov-tool`
: The coverage tools.

`lto-dump`
: Prints the contents of LTO object files.

`libgcc.a`
: The routines the compiler calls for operations the processor has no instruction for, and the
  unwinder.

## Built again later

[Native GCC](../03-bootstrap/100-gcc.md), at the end of `10_bootstrap`, is compiled by this
compiler to run on KDOS. It is still configured for `x86_64-kdos-linux-musl`, and it is complete:
shared libraries, POSIX threads, and libstdc++ built in. It applies the `gcc` port's
`t-linux64-lib.patch`, and it installs with the port's own paths (`--libexecdir=/usr/lib`).

The [gcc](#port-gcc) port, in `20_selfhost`, is the compiler KDOS ships. It is built inside the
chroot with no triplet, so it becomes `x86_64-pc-linux-musl` (see
[Two target names](intro.md#two-target-names)). It adds the Fortran and LTO front ends
(`--enable-languages=c,c++,fortran,lto`), POSIX threads, a hardened compiler binary
(`--enable-host-pie`, `--enable-host-bind-now`), build IDs on everything it links
(`--enable-linker-build-id`), and the system zlib. Its `libgcc` is built with `-g0`
(`LIBGCC2_DEBUG_CFLAGS`), its target libraries with `CFLAGS_FOR_TARGET` stripped of `-std=`, and it
identifies itself as `KDOS` (`--with-pkgversion`). It passes none of the `--disable-lib*` switches,
and it is the compiler every port from that point on is built with.

## See also

- [Part II](intro.md): the [cross toolchain]{.idx} and its environment
- [Cross binutils](binutils.md): the assembler and linker this compiler drives
- [musl libc](../03-bootstrap/020-musl-libc.md): the first thing this compiler builds
- [Build troubleshooting](../../kdos/05-developer/build-troubleshooting.md#a-language-standard-reaching-the-compilers-own-runtime): the `-std=` flag and GCC's runtime libraries

<!-- book-nav -->
---

*Part II. The cross toolchain, chapter 7.* Previous: [6. Cross binutils](binutils.md) · [Contents](../README.md) · Next: [Part III. The bootstrap](../03-bootstrap/intro.md)
