## About

GCC is KDOS's compiler for C, C++ and Fortran, with the runtime libraries programs in those
languages link: `libgcc_s`, `libstdc++`, `libgfortran`, `libgomp`, `libatomic` and the sanitizer
runtimes. Every later port's C, C++ and Fortran is compiled by this package, apart from the few recipes
that choose clang, and `cc` is a link to it.

It is the last port `20_selfhost` builds, after [gmp](#port-gmp), [mpfr](#port-mpfr),
[mpc](#port-mpc) and [zlib](#port-zlib), which it links, and with [binutils](#port-binutils)
already rebuilt. It is the second native GCC of the tree: [Part III](#03-bootstrap-100-gcc) built
one with the cross compiler, and this one is that compiler compiling itself inside the chroot. Its
target is the chroot's own `x86_64-pc-linux-musl`, not the bootstrap's `x86_64-kdos-linux-musl`,
and its install hook, `postinstall.sh`, clears away what the bootstrap left under the old name;
[What the gcc hook removes](../../../../docs/kfs/05-selfhost/intro.md#what-the-gcc-hook-removes)
lists it, and the reasons are in
[The toolchain rebuilt as packages](../../../../docs/kdos/05-developer/how-kdos-is-built.md#the-toolchain-rebuilt-as-packages-20_selfhost).
`30_foundation` names gcc again, so a changed recipe is rebuilt before anything else in that
phase compiles.

The source is configured and built in a `build` directory of its own inside the tree. GCC's
installation manual does not support building in the source directory itself; it recommends a
build directory outside the tree and says a subdirectory of it, as here, should work as well.

## Flags

- `--libexecdir=/usr/lib` — install the compiler's internal programs (`cc1`, `cc1plus`, `f951`,
  `collect2`, `lto1`, `lto-wrapper`) under `/usr/lib/gcc/<triple>/<version>/` instead of
  `/usr/libexec`. Part III's GCC is configured with the same switch.
- `--enable-languages=c,c++,fortran,lto` — build the C, C++ and Fortran front ends and the LTO
  front end, `lto1`, which `-flto` needs at link time. `gfortran` is the Fortran compiler of the
  numerical ports, [openblas](#port-openblas), [R](#port-R), [scipy](#port-scipy) and the rest.
- `--disable-bootstrap` — build the compiler once. A native GCC build otherwise compiles itself
  three times and compares the last two stages. The compiler building it here is the bootstrap's
  GCC of the same version.
- `--with-system-zlib` — use the installed zlib instead of the copy in GCC's source tree. GCC
  compresses LTO bytecode with it.
- `--without-zstd` — do not use zstd for LTO bytecode, and use zlib instead. [zstd](#port-zstd)
  does not exist in `20_selfhost`; with the switch, the compiler is the same whichever phase
  rebuilds it, as [binutils](#port-binutils)' is.
- `--without-isl` — build without the isl library, so the Graphite loop optimisations
  (`-floop-nest-optimize` and the rest) are not available. isl is not a port.
- `--enable-linker-build-id` — pass `--build-id` to the linker on every final link, so every
  executable and shared library GCC links carries a build ID even when a build's own link line
  leaves out the tree's `LDFLAGS`.
- `--disable-multilib` — build the 64-bit runtime libraries only, with no 32-bit (`-m32`) copies
  beside them.
- `--enable-threads=posix` — use POSIX threads for the C++ and Fortran runtimes' thread support.
  This is the default on Linux; the switch states it.
- `--enable-__cxa_atexit` — register C++ static destructors with `__cxa_atexit`, which musl
  provides. GCC already defaults to this on Linux targets; the switch states it.
- `--enable-default-pie` — compile and link every program as a position-independent executable
  unless told otherwise, so it is loaded at a random address.
- `--enable-default-ssp` — turn on `-fstack-protector-strong` for every compile. With
  `--enable-default-pie`, this is the part of the tree's hardening the compiler supplies by
  itself (see [The release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags)).
- `--enable-host-pie` — build GCC's own programs as position-independent executables.
- `--enable-host-bind-now` — link GCC's own programs with `-z now`, so every symbol is resolved at
  start and the GOT can be read-only.
- `-with-pkgversion="KDOS"` — print `KDOS` where `gcc --version` prints `GCC`: `gcc (KDOS)`. It
  is spelled with one dash, which autoconf's option parser accepts as it does two.
- `CFLAGS_FOR_TARGET` — the C flags for the libraries GCC builds for its target (`libgcc`,
  `libgfortran`, `libgomp`, the sanitizers). Unset, configure takes the build's `CFLAGS` and puts
  `-g` in front of them, so the packages would carry DWARF. The tree's `CFLAGS` are passed with
  their `-std=` option cut out, so these libraries compile under the C standard their own
  makefiles or GCC's default give them.
- `CXXFLAGS_FOR_TARGET` — the same for the C++ libraries, `libstdc++` above all: the tree's
  `CXXFLAGS`, with no `-g` added.
- `LDFLAGS_FOR_TARGET` — the tree's `LDFLAGS` for the shared runtime libraries, so `libstdc++.so`
  and the rest are linked with `-z now` and the other release link flags.
- `LIBGCC2_DEBUG_CFLAGS=-g0` — on both `make` lines. libgcc adds this variable, `-g` by default,
  after the flags it is given, so without it `libgcc` would be compiled with debug information
  whatever `CFLAGS_FOR_TARGET` says.

## Patches

- `t-linux64-lib.patch` — changes one line of `gcc/config/i386/t-linux64`: the operating-system directory of the 64-bit multilib becomes `../lib` instead of `../lib64`. GCC then installs its runtime libraries in `/usr/lib` and searches there, and nothing it links records a `/usr/lib/../lib64` `RPATH`. KDOS has one library directory, `/usr/lib`; `/usr/lib64` is a link to it (see [Syncing fs/](../../../../docs/kdos/05-developer/build-system.md#syncing-fs)).

## Notes

After `make install`, `build.sh` does three more things:

- It copies the manual pages of `gcc`, `g++` and `gfortran` and the info manuals of `gcc`,
  `gccint`, `cpp`, `cppinternals` and `gfortran` from the source, where the release carries them
  already built. GCC's build regenerates those three manual pages with `pod2man`, from
  [perl](#port-perl), and makes info manuals only when configure finds `makeinfo`, from
  [texinfo](#port-texinfo); both ports are built in `30_foundation`, after this one. Without
  them, `make install` writes a placeholder file for each of the three pages and installs none of
  those info manuals.
- It links `/usr/bin/cc` to `/usr/bin/gcc`, the name make's built-in rules and many makefiles call.
- It moves libstdc++'s pretty-printer hook, `libstdc++.so.6.*-gdb.py`, to
  `/usr/share/gdb/auto-load/usr/lib/`. gdb runs such a script only from its safe auto-load path, so
  beside the library it would be declined and every standard container would print as its
  internals.
