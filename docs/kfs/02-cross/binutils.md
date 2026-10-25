# Cross binutils

The first step of the build compiles GNU binutils, the assembler, the linker and the tools that
read and rewrite object files, to run in the container and write code for
`x86_64-kdos-linux-musl`. It is the step the cross GCC needs before it can be built, and it is
the unit of build time this book measures in. This page shows the commands of
`script/phases/00_cross/00_binutils.sh`, explains each configure switch, lists what it installs,
and compares it with the two later builds of the same source.

**Source:** the archive of the [binutils](#port-binutils) port\
**Installs into:** `build/cross`\
**Approximate build time:** 1 SBU, by definition; 43 seconds on the machine that took this book's
snapshot

## Installation of cross binutils

<!-- kfs:include script/phases/00_cross/00_binutils.sh lines=24-39 pin=514c2d0cae2a -->

`extract_port_source` unpacks the port's archive into `build/tmp` and prints the directory it made;
[Part III](../03-bootstrap/intro.md#how-a-step-finds-its-source) describes it. binutils is built in
a `build` directory of its own, as its documentation asks, so the source tree stays clean.

### The meaning of the configure options

`--target=$KDOS_TARGET`
: Build tools that run here and write for `x86_64-kdos-linux-musl`. Every program is installed as
  `build/cross/bin/x86_64-kdos-linux-musl-ld` and the like, the names a configure script given
  `--host` looks for. The ten that a compiler driver runs, `as`, `ld` and `ar` among them, are
  installed a second time under their plain names in `build/cross/x86_64-kdos-linux-musl/bin`,
  which is where the cross GCC looks for its assembler and linker.

`--with-sysroot=$SYSROOT`
: Make `build/fs` the linker's root. Its default library directories, `/usr/lib` among them, and
  every absolute path in a linker script are looked up under `build/fs`, so `-lc` finds
  `build/fs/usr/lib/libc.so` and never the container's.

`--prefix=$CROSS_SYSROOT`
: Install under `build/cross`.

`--disable-nls`
: Build no message catalogues. A build log is read in English, and nothing in this toolchain
  outlives the bootstrap.

`--disable-werror`
: Keep the warnings that the container's compiler raises in binutils' sources as warnings, so a
  newer warning does not stop the build.

`--disable-multilib`
: KDOS has one ABI, 64-bit x86. The switch belongs to the top-level configure that binutils shares
  with GCC, where it governs the libraries built for the target. binutils builds none, so it changes
  nothing here; it keeps this line saying what the GCC line beside it says.

`--enable-new-dtags`
: Make the linker write a `-rpath` as `DT_RUNPATH` rather than the older `DT_RPATH`. The dynamic
  linker searches `DT_RUNPATH` after `LD_LIBRARY_PATH`, so a run path never overrides the
  environment.

`--enable-gprofng=no`
: Do not build gprofng, the newer profiler. binutils' configure already leaves it out for every
  `*-musl*` target, so this states the result rather than changing it.

`--enable-default-hash-style=gnu`
: Write only the GNU symbol hash table, `.gnu.hash`, into shared objects and dynamic executables.
  On a Linux target the linker otherwise writes the classic SysV `.hash` as well. musl's dynamic
  linker reads `.gnu.hash` when it is present, so the second table is never used.

`make` and `make install` take no options. The job count comes from `MAKEFLAGS`, which
`common.env` sets (see [Part II](intro.md#the-environment)).

## Contents of cross binutils

**Installed programs**, in `build/cross/bin`, each prefixed `x86_64-kdos-linux-musl-`: [`addr2line`]{.idx},
[`ar`]{.idx}, [`as`]{.idx}, [`c++filt`]{.idx}, [`elfedit`]{.idx}, [`gprof`]{.idx}, [`ld`]{.idx}, [`ld.bfd`]{.idx}, [`nm`]{.idx}, [`objcopy`]{.idx}, [`objdump`]{.idx}, [`ranlib`]{.idx},
[`readelf`]{.idx}, [`size`]{.idx}, [`strings`]{.idx} and [`strip`]{.idx}.

**Installed directory:** `build/cross/x86_64-kdos-linux-musl`, with unprefixed copies of `ar`,
`as`, `ld`, `ld.bfd`, `nm`, `objcopy`, `objdump`, `ranlib`, `readelf` and `strip` in `bin`, and the
linker scripts in `lib/ldscripts`.

`as`
: The assembler: turns the assembly GCC writes into object files.

`ld`, `ld.bfd`
: The linker, under its generic name and its implementation's name. It joins object files and
  libraries into an executable or a shared object.

`ar`, `ranlib`
: Make static archives and index them.

`nm`, `objdump`, `readelf`, `size`, `strings`, `addr2line`, `c++filt`
: Read object files: symbols, sections, headers, sizes, printable strings, the source line of an
  address, and demangled C++ names.

`objcopy`, `strip`, `elfedit`
: Rewrite object files: copy and convert sections, remove symbols, change ELF header fields.

`gprof`
: The call-graph profiler.

## Built again later

The same source is built twice more, and each build replaces this one in its own place.

[Native binutils](../03-bootstrap/090-binutils.md), in `10_bootstrap`, is compiled by the cross GCC
to run on KDOS. It keeps every switch above except the three that make a [cross toolchain]{.idx}:
`--target` and `--with-sysroot` go, `--host=$KDOS_TARGET` comes in, and it installs into
`build/fs/usr`.

The [binutils](#port-binutils) port, in `20_selfhost`, is the one KDOS ships. It is built inside
the chroot with no triplet, and it adds what a system's binutils needs: shared `libbfd` and
`libopcodes` (`--enable-shared`), the linker plugin interface and LTO support (`--enable-plugins`,
`--enable-lto`), deterministic archives (`--enable-deterministic-archives`), 64-bit BFD on every
host (`--enable-64-bit-bfd`), and the system zlib and xxHash (`--with-system-zlib`, `--with-xxhash`).
It leaves out `--disable-multilib`, `--enable-new-dtags`, `--enable-gprofng=no` and
`--enable-default-hash-style=gnu`. The last has a visible effect: what the shipped linker links carries both `.hash` and `.gnu.hash`,
where everything linked by the bootstrap toolchain carries only `.gnu.hash`. In the snapshot's
tree, `/usr/bin/ls` has both and `/usr/bin/kpkg` only the GNU table.

## See also

- [Part II](intro.md): the cross toolchain and its environment
- [Cross GCC, pass 1](gcc.md): the compiler that drives these tools
- [How KDOS is built](../../kdos/05-developer/how-kdos-is-built.md#the-cross-toolchain-00_cross): where this step sits in the whole build

<!-- book-nav -->
---

*Part II. The cross toolchain, chapter 6.* Previous: [Part II. The cross toolchain](intro.md) · [Contents](../README.md) · Next: [7. Cross GCC, pass 1](gcc.md)
