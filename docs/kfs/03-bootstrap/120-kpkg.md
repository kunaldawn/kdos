# kpkg

kpkg is KDOS's package manager: it reads a port's recipe, runs its `build.sh`, packs the result and
installs it with a record of every file. From `20_selfhost` on, every package the build installs
goes through a `kpkg install` run inside `build/fs`, so kpkg has to be in the root before the first
of them. It
cannot be a port, because building a port needs kpkg. `120_kpkg.sh` therefore compiles it by hand,
with the cross compiler, straight from the tree's own C sources, and installs it into `build/fs`
under its five names.

**Approximate build time:** 0.2 SBU

## What it is built from

kpkg is the program in `src/system/kdos-kpkg` and three of KDOS's own libraries from `src/libs`:

- `libkbase`, the shared base: files, strings, processes, SHA-256;
- `libkpkg`, the configuration, the package database, the ports tree, the solver and the recipe
  hashes;
- `libksig`, Ed25519 signing and verification for the binary repository, which carries its
  cryptography as vendored source in `libksig/monocypher/`.

None of the four links anything but the C library, which is what lets kpkg be built here, against
musl and the kernel headers alone. That rule, and what breaks if a library ever needs a real `-l`,
is [the constraint](../../kdos/05-developer/c-libraries.md#the-constraint) of KDOS's C libraries.
kpkg runs bash to execute a recipe's `build.sh`, but that is a need of each build at run time, not
of the link.

## Installation of kpkg

The step names its sources and its output, a scratch file under `build/tmp`:

<!-- kfs:include script/phases/10_bootstrap/120_kpkg.sh lines=30-32 pin=57e6fce25b98 -->

### The mark is a hash

The other package steps of the bootstrap skip themselves when their mark file exists. This one
writes a SHA-256 into its mark instead, over every `.c` and `.h` file and [`kpkg.conf`]{.idx} in
the four source directories and over the step script itself, and skips only when the stored hash still matches:

<!-- kfs:include script/phases/10_bootstrap/120_kpkg.sh lines=39-47 pin=06f2d23891ca -->

kpkg is not a port, so no [recipe hash]{.idx} covers it and no later phase rebuilds it. Without this hash a
tree whose kpkg predates a change to its sources would go on installing every later package with
the old logic. The script is in the sum because its compile line sets the binary's flags.

### The compile line

The whole program is one compiler invocation: the libraries are not archives to link but source
files compiled into the program alongside its own.

<!-- kfs:include script/phases/10_bootstrap/120_kpkg.sh lines=49-56 pin=85f50342e902 -->

The line does not use the `CFLAGS` or `LDFLAGS` of `script/env/host.env`. Every flag kpkg is built
with is here, and because no later phase rebuilds it, the line carries the tree's hardening itself.

`-O2 -pipe`
: Optimise, and pass data between the compiler's stages through pipes rather than temporary files.

`-std=gnu11 -D_GNU_SOURCE`
: The C dialect the libraries are written in, with musl's GNU and Linux extensions declared.

`-Wall -Wextra`
: The full set of ordinary warnings.

`-fstack-protector-strong`
: A canary in every function with a local array or a local whose address is taken, checked before
  the function returns.

`-fstack-clash-protection`
: A large stack allocation touches each page as it grows, so it cannot step over the guard page
  into other memory.

`-Wl,-O1,--as-needed`
: The linker optimises its output, and records a shared library as needed only when the program
  uses it, which here is musl alone.

`-Wl,-z,now`
: Every symbol is bound when the program starts rather than at its first call, so the table of
  resolved addresses can be read-only for the whole of the program's life.

`-Wl,-z,pack-relative-relocs`
: Relative relocations are stored in the compact `DT_RELR` form, which musl's loader reads.

`-Wl,--build-id=sha1`
: The binary carries a build ID note, a SHA-1 of its contents, so the same sources give the same
  ID. The cross compiler adds no build ID of its own.

`-I…`
: Each library's directory and the program's own, for their headers.

`-o "$OUT"`
: Write to `build/tmp/kdos-kpkg`. The source list is every `.c` file of the program, the three
  libraries and Monocypher, by glob.

### Installing it under five names

<!-- kfs:include script/phases/10_bootstrap/120_kpkg.sh lines=58-68 pin=e13f4113c9ea -->

`install -Dm755` puts the binary at `/usr/bin/kpkg`. kpkg is one program answering to five names
and decides what to do from the name it was run as, so the loop makes `kpkgadd`, `kpkgbuild`,
`kpkgdel` and `kpkgdepends` symbolic links to it. What each name does is in
[kpkg](../../kdos/03-architecture/packaging.md#kpkg).

`kpkg.conf` is copied to [`/etc/kpkg.conf`]{.idx}. It sets where kpkg finds ports (`PORT_REPO`, the
recipe tree at `/ports/core` and the `src/` areas under `/ports/src`), where it keeps sources,
packages and its working directory under `/var/cache/kpkg`, and where the package database lives,
`/var/lib/kpkg/db`. The last line writes the source hash into the mark.

## Contents of kpkg

**Installed programs:** kpkg, kpkgadd, kpkgbuild, kpkgdel and kpkgdepends (the last four symbolic
links to kpkg)

**Installed file:** `/etc/kpkg.conf`

## Built again later

It is not. `src/system/kdos-kpkg` has no recipe, so no phase builds kpkg as a package and no
package owns `/usr/bin/kpkg`: the kpkg on a finished KDOS image is the binary this step built. A
change to kpkg or to one of its three libraries reaches the tree through the mark's hash, by running
this step again; [Rebuilding one thing](../../kdos/05-developer/developing.md#rebuilding-one-thing)
gives the command line. [Anatomy of a kpkg install](../04-chroot/kpkg-install.md) follows what this
program does with its first recipe.

## See also

- [Make](110-make.md), the step before this one, and [kinstall](130-kinstall.md), the step after it
- [Packaging](../../kdos/03-architecture/packaging.md): kpkg's commands, packages, ownership and the
  recipe hash
- [libkpkg](../../kdos/05-developer/c-libraries.md#libkpkg) and
  [libksig](../../kdos/05-developer/c-libraries.md#libksig): the two libraries kpkg is built on

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 28.* Previous: [27. Make](110-make.md) · [Contents](../README.md) · Next: [29. kinstall](130-kinstall.md)
