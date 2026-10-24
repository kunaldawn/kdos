# Self-hosting

<!-- kfs:phase 20_selfhost -->

`20_selfhost` is the first phase that runs inside the [chroot]{.idx}, and the first in which [kpkg]{.idx} builds
anything. Its list is short: the fortify headers, tar, the C library, zlib, the assembler and
linker, the compiler, and the few tools their builds run, each a port rebuilt over the files
[Part III](../03-bootstrap/intro.md) installed by hand. This part explains what the phase sets out
to do, the environment it runs in, why its list holds these ports and in this order, how the
compiler's name changes on the way, and what the `gcc` package's install hook clears away at the
end. The package pages follow, in chapters by shelf, in the shelves' own order, and within a shelf
in the order [kpkgdepends]{.idx} builds them.

## What self-hosting means here

When [Part III](../03-bootstrap/intro.md) ends, the root already holds a native GCC: it runs on
KDOS and produces programs for KDOS. But that compiler, and the musl and binutils beside it, were
compiled in the build container by the cross compiler of [Part II](../02-cross/intro.md). This phase
uses the bootstrap compiler, running inside the chroot, to compile musl, binutils and GCC again
from their ports. When it ends, the C library, the assembler, the linker and the compiler that
every later phase uses were compiled on KDOS, inside KDOS, and no file of theirs came out of the
container. That is what the book means by [self-hosting]{.idx}; the comparison with how other
distributions produce their toolchains is in
[How KDOS differs](../../kdos/01-philosophy/how-kdos-differs.md#self-hosting).

Two limits are worth knowing exactly:

- The [gcc](#port-gcc) port passes `--disable-bootstrap`, so GCC's build compiles the compiler once,
  with the compiler it finds, instead of three times with the last two stages compared. The
  packaged GCC is therefore the output of the bootstrap GCC, not of itself.
- No phase compiles that GCC again unless something forces it. `30_foundation` names `gcc` again,
  but kpkg skips a port that is installed from an [unchanged recipe]{.idx term="recipe hash"} (see
  [Deciding: resolution and the recipe hash](../04-chroot/kpkg-install.md#deciding-resolution-and-the-recipe-hash)).
  What the packaged GCC does compile is every port from `30_foundation` on.

## Ownership

The phase has a second job, which matters as much as the first. Nothing the bootstrap installed
belongs to a package: there is no database entry for `/usr/lib/libc.so` or `/usr/bin/gcc`, so
nothing could upgrade, verify or remove them. When kpkg installs a package over a path that no
package claims, it [adopts]{.idx term="path adoption"} the path instead of refusing it, and the path enters the new
package's [manifest]{.idx} (see
[Who owns a file](../../kdos/03-architecture/packaging.md#who-owns-a-file) and
[Adopting: kpkgadd](../04-chroot/kpkg-install.md#adopting-kpkgadd)). Each port of this phase
turns a set of hand-installed files into package files. `30_foundation` does the same for the rest
of the bootstrap's programs; [What the bootstrap leaves](../03-bootstrap/what-you-have.md) lists
them.

Adoption covers only a path the port installs again. A bootstrap file that the port does not
install stays unowned for good. That is why the bootstrap's binutils and GCC steps delete the
[libtool archives]{.idx term="libtool archive"} and the `libstdc++` debugger script they install, files no package will ever
claim, and why the `gcc` package carries the install hook described
[below](#what-the-gcc-hook-removes).

## The environment

Every step of the phase runs [`kpkg install <port>`]{.idx term="kpkg install"} in the chroot with this environment loaded,
from [`script/phases/20_selfhost/phase.env`]{.idx term="phase.env"}:

<!-- kfs:include script/phases/20_selfhost/phase.env lines=23-29 pin=c28d63f18d12 -->

[`CHROOT=1`]{.idx term="CHROOT"} makes the orchestrator run the phase's steps through the chroot wrapper. It is read from
the file's own text, which is why it is set here and not in the shared file (see
[The phase environment](../../kdos/05-developer/build-system.md#the-phase-environment)).

[`script/env/chroot.env`]{.idx term="chroot.env"} is the environment every chroot phase shares, described in
[Entering the chroot](../04-chroot/entering.md#the-phase-environment). Several of its lines do
nothing yet in this phase, because what they wait for is not installed:

- The `ninja` and `meson` wrappers in `script/bin` go on `PATH` only once `/usr/bin/ninja` exists.
  [ninja](#port-ninja) is a `30_foundation` port.
- The [compiler cache]{.idx term="ccache"} is switched on only once `/usr/bin/ccache` exists, which is also
  `30_foundation`'s doing, so nothing here is cached. Until then the file sets [`CCACHE_DISABLE=1`]{.idx term="CCACHE_DISABLE"}.
- The switches that make Rust [`-sys` crates]{.idx} link the system library are exported only from
  `40_lang` on: `chroot.env` reads the path of the `phase.env` that sourced it.

The release flags themselves do apply. Every port of this phase is compiled with the [`CFLAGS`]{.idx},
[`CXXFLAGS`]{.idx} and [`LDFLAGS`]{.idx} of
[]{.idx term="release flags"}[the release flags](../../kdos/05-developer/writing-ports.md#the-release-flags), and
[fortify-headers](#port-fortify-headers) comes first so that [`-D_FORTIFY_SOURCE=2`]{.idx term="_FORTIFY_SOURCE"} has the headers
it needs from the second port on.

The last line is the one this phase adds. `chroot.env` names the compiler outright, [`CC=gcc`]{.idx term="CC"} and
[`CXX=g++`]{.idx term="CXX"}, so that no configure script can switch to clang once clang is installed, and it sets
[`ac_cv_prog_cxx_cxx11`]{.idx term="ac_cv_prog_cxx_cxx11"} to empty, which tells an autoconf configure that the C++ compiler needs no
extra option. This phase is building that compiler, with only the bootstrap GCC installed, so it
unsets all three, and each configure finds its own compiler and chooses its own C++ mode.

## The list and its order

The phase's whole list is one file:

<!-- kfs:include script/phases/20_selfhost/packages.txt -->

[kdosbuild]{.idx} resolves it the way `kpkgdepends` does: each name in turn, with the ports it depends on
ahead of it, and nothing that is already installed and current. Ports come in that the list
never names: [xxhash](#port-xxhash) ahead of binutils, and [gmp](#port-gmp), [mpfr](#port-mpfr) and
[mpc](#port-mpc), which GCC links. gawk depends on GMP and MPFR as well, so those two arrive
ahead of gawk, and MPC follows them just before GCC. No phase's list names any of them, so this phase is the only one that installs them.
[ncurses](#port-ncurses) and [readline](#port-readline) come in ahead of gawk the same way, and
`30_foundation` names them again. `20_selfhost` is the one package phase that the rule
[every phase installs exactly its list](../../kdos/05-developer/build-system.md#every-phase-installs-exactly-its-list)
does not hold to.

What each listed port is to the phase:

| Port | Why it is here |
|---|---|
| [fortify-headers](#port-fortify-headers) | The checking wrappers in `/usr/include/fortify` that the release flags compile every later port against. A port built before them is built unchecked |
| [tar](#port-tar) | kpkg runs `tar` to unpack every later source, to pack every package and to install it, so it is the first of the bootstrap's programs to become a package |
| [musl](#port-musl) | The C library, rebuilt natively with `--enable-optimize`, and with the do-nothing `ldconfig` script that install scripts call |
| [zlib](#port-zlib) | Linked by binutils and GCC, both of which are configured `--with-system-zlib` |
| [binutils](#port-binutils) | The assembler and linker, native to the new triplet and installed with `tooldir=/usr` |
| [diffutils](#port-diffutils), [m4](#port-m4), [gawk](#port-gawk) | Tools the GMP and GCC builds run: GMP's configure asks for `m4`, GCC's makefiles run `$(AWK)` scripts, and its `move-if-change` compares files with `cmp` |
| [gcc](#port-gcc) | The C, C++ and Fortran compiler that every later phase uses |

The bootstrap built `diffutils` and `gawk` as well, `gawk` configured `--without-readline` and
`--without-mpfr`. Here both become packages compiled with the release flags, and `gawk` is
configured `--with-mpfr` and `--with-readline`, which is why GMP, MPFR and readline come in ahead
of it. The rest of the bootstrap's userland waits for `30_foundation`.

## The triplet changes

The bootstrap's tools were built for a [target]{.idx term="target triplet"} the build names itself: `script/env/host.env` sets
[`KDOS_TARGET=x86_64-kdos-linux-musl`]{.idx term="KDOS_TARGET"}, and the bootstrap compiles with the tools of that name. The
binutils and GCC steps of Part III installed copies of themselves under that name too: target-named
drivers such as `/usr/bin/x86_64-kdos-linux-musl-gcc`, a tool directory
`/usr/x86_64-kdos-linux-musl`, and GCC's private tree `/usr/lib/gcc/x86_64-kdos-linux-musl`.

The ports of this phase pass no `--build`, `--host` or `--target`. Their configure scripts ask the
source's own [`config.guess`]{.idx}, which on an x86_64 Linux machine prints `x86_64-pc-linux-` followed by
the C library it detects, here `musl`. Every package from here on is native to
[x86_64-pc-linux-musl]{.idx}; the configure logs of binutils and GCC in this phase say

```text
checking build system type... x86_64-pc-linux-musl
checking host system type... x86_64-pc-linux-musl
```

and the packaged compiler keeps its private files in `/usr/lib/gcc/x86_64-pc-linux-musl` and
answers [`gcc -dumpmachine`]{.idx term="-dumpmachine"} with that name. The LLVM ports of [Part VII](../07-compilers/intro.md)
take their default target from the same answer, `$(cc -dumpmachine)`.

The new name has a consequence for ownership. The packaged GCC writes `/usr/bin/gcc`,
`/usr/lib/libstdc++.so` and the other shared names over the bootstrap's files and adopts them, but
nothing it installs carries the old name, so the bootstrap's target-named trees are never adopted
and never replaced.

## What the gcc hook removes

The [gcc](#port-gcc) port is the one port of the phase with an [install hook]{.idx}. It is a
[`postinstall.sh`]{.idx} beside the recipe, which travels inside the package and runs each time the package
is installed (see [postinstall.sh](../../kdos/05-developer/writing-ports.md#postinstallsh)):

<!-- kfs:include ports/core/toolchain/gcc/postinstall.sh -->

It deletes the bootstrap compiler's private tree, its binutils tool directory, the C++ headers it
kept under its own name, every target-named program in `/usr/bin` and the `gccinstall.info`
manual. No package owns any of it, so without the hook a second toolchain would stay in the root
for good and reach the image. The hook works on [`PKG_ROOT`]{.idx}, the root [kpkgadd]{.idx} is installing into.
`rm -f` and `rm -rf` pass over a path that does not exist, so on every later install of `gcc` the
hook finds nothing and changes nothing.

`$version` in the hook is the recipe's `version`: kpkg writes the recipe's metadata, `name` and
`version` among it, at the head of the hook it puts into the package. The C++ header path therefore names the version being installed,
and it matches the bootstrap's because `100_gcc.sh` takes its source and version from the same
port (see [How a step finds its source](../03-bootstrap/intro.md#how-a-step-finds-its-source)).

## When the phase is over

The tree that `20_selfhost` leaves has a package database with the phase's ports in it, a compiler
and C library that are packages, and no file with [`x86_64-kdos-linux-musl`]{.idx} in its path. Everything
else the bootstrap installed is still unowned: the minimal builds of bash, coreutils, grep, sed,
findutils, patch, make, xz and gzip; the Linux API headers, which the `linux` port of `60_kernel`
takes over; and kpkg and kinstall, which no phase's list names. [Part VI](../06-foundation/intro.md) rebuilds the userland
as full ports before anything needs what the bootstrap left out.

The phase writes a [snapshot]{.idx} of `fs` when it ends, so a later failure can restart from this tree
(see [Snapshots](../../kdos/05-developer/build-system.md#snapshots)).

## See also

- [The toolchain rebuilt as packages](../../kdos/05-developer/how-kdos-is-built.md#the-toolchain-rebuilt-as-packages-20_selfhost): the phase in the whole journey
- [What the bootstrap leaves](../03-bootstrap/what-you-have.md): the tree this phase starts from
- [Anatomy of a kpkg install](../04-chroot/kpkg-install.md): the steps every port below goes through
- [GCC](../03-bootstrap/100-gcc.md), [binutils](../03-bootstrap/090-binutils.md) and [musl](../03-bootstrap/020-musl-libc.md): the bootstrap builds these ports replace
- [Reading a package page](../00-front/reading-a-package-page.md): how the pages below are laid out
- [Part VI](../06-foundation/intro.md): the next phase

<!-- book-nav -->
---

*Part V. Self-hosting.* Previous: [32. Anatomy of a kpkg install](../04-chroot/kpkg-install.md) · [Contents](../README.md) · Next: [Part VI. The build foundation](../06-foundation/intro.md)
