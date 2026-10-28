# What the bootstrap leaves

When `130_kinstall.sh` finishes, `build/fs` is a small but complete Linux userland: a C library, a
shell, the GNU command line, a native compiler and linker, make, and the package manager. You could
`chroot` into it and compile a program. This page takes stock of that tree before
[Part IV](../04-chroot/intro.md) enters it: what is there, what is still missing, and why the first
package phase builds much of it a second time.

## What is in the tree

The layout comes first. [`000_file_system.sh`](000-file-system.md) made the directory skeleton and
the merged-`/usr` links (`/bin`, `/sbin`, `/lib` and `/lib64` point into `/usr`), and copied the
repository's `fs/` overlay over it, which carries KDOS's own configuration and makes `/usr/lib64`
a link to `lib`. Everything every later step installed went into those directories.

On that layout the steps of this part left:

| Area | Steps | What it gives the tree |
|---|---|---|
| Kernel interface and C library | [Linux headers](010-linux-headers.md), [musl](020-musl-libc.md) | `/usr/include/linux` and the rest of the kernel's user-space headers, musl's `libc.so` and its dynamic loader |
| C++ and terminals | [libstdc++](030-libstdcxx.md), [ncurses](040-ncurses.md) | The C++ standard library and the terminal library readline and bash link |
| Unpacking sources | [xz](050-xz.md), [gzip](060-gzip.md), [tar](061-tar.md) | What a build inside the chroot opens its `.tar.xz` and `.tar.gz` with |
| The command line | [coreutils](062-coreutils.md), [diffutils](063-diffutils.md), [findutils](064-findutils.md), [gawk](065-gawk.md), [grep](066-grep.md), [patch](067-patch.md), [sed](068-sed.md) | The tools every `build.sh` and configure script runs |
| The shell | [readline](070-readline.md), [bash](080-bash.md) | `/usr/bin/bash`, with `/usr/bin/sh` a link to it |
| The toolchain | [binutils](090-binutils.md), [GCC](100-gcc.md), [make](110-make.md) | A native assembler, linker, C and C++ compiler, `cc`, and make |
| KDOS's own | [kpkg](120-kpkg.md), [kinstall](130-kinstall.md) | The package manager under five names with `/etc/kpkg.conf`, and the installer |

Beside `build/fs`, two more directories carry the phase's result. `build/cross` still holds the
cross toolchain of [Part II](../02-cross/intro.md), which nothing inside the chroot uses, and
`build/mark/bootstrap/` holds one mark per step, which makes a later run of `10_bootstrap` skip
every step whose mark is present. A snapshot of this phase, when a run writes one, archives exactly
those three paths, `cross`, `fs` and `mark`, as `script/phases/10_bootstrap/phase.env` declares
them; it leaves out `fs/tmp`, `fs/dev`, `fs/proc`, `fs/sys`, `fs/run` and the two mount points,
`fs/kdos` and `fs/ports`. See [Snapshots](../../kdos/05-developer/build-system.md#snapshots).

## What is not there yet

**No package owns anything.** Every file in the tree was put there by a step script with
`make DESTDIR=$SYSROOT install`, `install` or `cp`. kpkg exists but has installed nothing, so there
is no package database under `/var/lib/kpkg/db`, and no record of which file came from where.

**The toolchain is minimal.** The native GCC compiles C and C++ only, and has none of the
atomic-operations, OpenMP, quad-precision or sanitizer runtimes. It and binutils also installed
copies of themselves under their target name, `x86_64-kdos-linux-musl`, which the compiler that
replaces them never uses.

**The tools carry almost no optional features.** Beyond what earlier steps built, no step found
an optional library to link: coreutils without ACLs, extended attributes, libcap, GMP or OpenSSL;
grep without `-P`; gawk without readline or MPFR; patch and sed without ACL or extended-attribute
support. Native-language support is off in every step that passes `--disable-nls`: libstdc++, xz,
tar, coreutils, diffutils, findutils, gawk, grep, sed, bash, binutils, GCC and make, so no step
installs a message catalogue under `/usr/share/locale`; ncurses, gzip, patch and readline have no
native-language support to switch. The package steps compile with the plain `-O2 -pipe` of
`script/env/host.env`, not the
[release flags](../../kdos/05-developer/writing-ports.md#the-release-flags); only kpkg and kinstall
carry hardening flags, on their own compile lines, and nothing has been compiled against
[`fortify-headers`]{.idx}. readline and bash are built without their upstream patch
series.

**Whole parts of a build environment are absent.** There is no zlib, no m4, no Perl, no Python, no
`pkg-config`, no autotools, no CMake or Meson, and no gettext. There is no init system, kernel or
boot loader either: the tree can be entered as a chroot but not booted. Every one of these is a port
in a later phase.

## Why `20_selfhost` builds it again

The next phase is the first one inside the chroot, and its list rebuilds the toolchain as packages:
`fortify-headers`, `tar`, `musl`, `zlib`, `binutils`, `diffutils`, `m4`, `gawk` and `gcc`, with
the libraries they depend on. It has two jobs.

The first is ownership. When kpkg installs a package over a path no package claims, it adopts the
path (see [Who owns a file](../../kdos/03-architecture/packaging.md#who-owns-a-file)), so each of
those ports turns files this part left into files a package records in its manifest. `30_foundation`
does the same for the rest of the bootstrap's programs, bash among them. kpkg and kinstall stay as
this part built them: neither has a port in any phase list.

The second is [self-hosting]{.idx}. After `20_selfhost` the C library and the compiler are themselves
compiled by a compiler running on KDOS, inside KDOS, and depend on nothing the container built.
That compiler is native to `x86_64-pc-linux-musl`, and the `gcc` package's install hook deletes the
`x86_64-kdos-linux-musl` trees the bootstrap left. The mechanism, and the full list of what the
phase installs, are in
[The toolchain rebuilt as packages](../../kdos/05-developer/how-kdos-is-built.md#the-toolchain-rebuilt-as-packages-20_selfhost).

Each step page of this part ends with a "Built again later" section saying where its package is
rebuilt and how the port build differs.

## See also

- [kinstall](130-kinstall.md), the last step of this part
- [Entering the chroot](../04-chroot/entering.md): how the next phase runs inside this tree
- [Part V](../05-selfhost/intro.md): the self-hosting phase
- [A minimal KDOS](../../kdos/05-developer/how-kdos-is-built.md#a-minimal-kdos-10_bootstrap): the
  phase as `docs/kdos` describes it

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 30.* Previous: [29. kinstall](130-kinstall.md) · [Contents](../README.md) · Next: [Part IV. The chroot and kpkg](../04-chroot/intro.md)
