# Glossary

This glossary lists the KDOS terms a reader of this book meets, in alphabetical order, each with the
sense this book uses it in and a link to the entry or chapter that defines it fully. Most of them
are defined at length in [the KDOS book's glossary](../../kdos/06-reference/glossary.md), which is
the authority; a term that belongs to this book alone, such as *tier* or *SBU*, links to the
chapter of this book that defines it. A linked word in *italics* inside an entry has an entry of its own.

## Terms

### area

One of the four directories under `src/` that hold ports of KDOS's own: `src/system/`,
`src/art/`, `src/desktop/` and `src/daemons/`. A port of KDOS's own sits in an area rather than on a
[*shelf*](#shelf), and its pages are in the chapter named `src-<area>` of the part for its phase.
See [area in the KDOS glossary](../../kdos/06-reference/glossary.md#area).

### binhost

A directory of prebuilt packages with a signed index, which `kpkg binhost` installs from when a
package's hashes match the machine's. The build in this book makes one only when asked, with
`KDOS_MAKE_BINHOST=1`. See [binhost in the KDOS glossary](../../kdos/06-reference/glossary.md#binhost).

### box

A rootless container that one application, or one working environment, runs in. Boxes belong to
the running desktop, not to the build, and the medium carries none. This book meets them as what
the base pack `kdos` is for: a box built over it gives a running KDOS a clean KDOS to build ports
in. See [box in the KDOS glossary](../../kdos/06-reference/glossary.md#box) and
[Boxes](../16-wiki/boxes.md).

### build-state snapshot

The files under `docs/kfs/data/` that `make book-data` writes from a built tree: each package's
installed version, size, file count and build time, its programs, libraries, pkg-config modules and
manual-page summaries, the size of every source, and the stamp that names the checkout's commit
when the data was gathered. The package pages take every figure about a built package
from it. Not to be confused with a phase [*snapshot*](#snapshot). See
[Reading a package page](../00-front/reading-a-package-page.md#the-header).

### chroot

The root being built, `build/fs`, entered as `/` with `script/chroot/exec.sh`, with a cleared
environment. Every phase from `20_selfhost` on runs its commands there. See
[chroot in the KDOS glossary](../../kdos/06-reference/glossary.md#chroot) and
[Entering the chroot](../04-chroot/entering.md).

### cross toolchain

The compiler and linker `00_cross` builds into `build/cross`, which run in the container and
produce programs for KDOS, before KDOS exists to run a compiler of its own. See
[cross toolchain in the KDOS glossary](../../kdos/06-reference/glossary.md#cross-toolchain) and
[Part II](../02-cross/intro.md).

### kpkg

The package manager, which reads a [*recipe*](#recipe), builds it into a package and installs it. Every
package page stands for one `kpkg install`. See [kpkg in the KDOS glossary](../../kdos/06-reference/glossary.md#kpkg)
and [Anatomy of a kpkg install](../04-chroot/kpkg-install.md); on a running system, see
[kpkg](../16-wiki/kpkg.md).

### kpkgbuild

The metadata file of a [*port*](#port), `key = value` lines that kpkg parses and never runs. See
[kpkgbuild in the KDOS glossary](../../kdos/06-reference/glossary.md#kpkgbuild).

### kpkgdepends

The tool that turns a list of ports into the order they must be installed in, following their
`depends =` lines. Within each shelf's chapter, the pages are in the order it resolves. See
[kpkgdepends in the KDOS glossary](../../kdos/06-reference/glossary.md#kpkgdepends).

### pack

One application, runtime, base or dataset as a single signed file, an EROFS image with metadata
appended. `70_image` creates the directories packs are installed into and, with
`KDOS_PACK_KDOS=1`, packs the built root itself as the base pack `kdos`. See
[pack in the KDOS glossary](../../kdos/06-reference/glossary.md#pack) and [Packs](../16-wiki/packs.md).

### package lint

The checks kpkg runs over the [*staging tree*](#staging-tree) before it rolls a package. It refuses
the package for a path outside the system's layout (`usr/lib64`, `usr/etc` or `usr/local`, among
others), a file mode it does not allow, a `.pc` file that cannot be resolved, or a program with an
executable stack or an `RPATH` into a build tree, and it warns about lesser faults, such as a plain
`/usr/lib` `RPATH`. See
[What the package lint refuses](../../kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)
and [The package lint](../../kdos/03-architecture/packaging.md#the-package-lint).

### package list

The list of ports a [*phase*](#phase) installs, kept in its `packages.txt` or in the files of its
`packages.d/` directory. The comment above a port's name in it is what a package page quotes as
*Why it is in KDOS*. See [package list in the KDOS glossary](../../kdos/06-reference/glossary.md#package-list)
and [About, and why it is in KDOS](../00-front/reading-a-package-page.md#about-and-why-it-is-in-kdos).

### package store

`build/pkgstore`, where the build keeps every package it makes when `KDOS_PKG_STORE` is on, and
from which it installs a port whose recipe, environment and dependencies match a package made
before, instead of building it again. A port installed from the store records no new build time:
the book keeps the figure of the last run that built it. See
[The package store](../../kdos/03-architecture/packaging.md#the-package-store) and
[Caches and the package store](../01-preparing/build-tree.md#caches-and-the-package-store).

### phase

One stage of the build, a directory under `script/phases/` such as `41_system`, run in name order
by kdosbuild. Three phases are numbered step scripts; the others are each a [*package list*](#package-list). Every
phase has a part of this book to itself. See
[phase in the KDOS glossary](../../kdos/06-reference/glossary.md#phase) and
[kdosbuild](../01-preparing/kdosbuild.md#phases).

### port

One piece of software as the tree describes it: a directory holding a [*kpkgbuild*](#kpkgbuild) and a
`build.sh`, filed on a [*shelf*](#shelf) under `ports/core/` or in an [*area*](#area) under `src/`, and known
everywhere by its bare name. Every port has a page in this book. See
[port in the KDOS glossary](../../kdos/06-reference/glossary.md#port).

### recipe

A port's build files: its [*kpkgbuild*](#kpkgbuild), its `build.sh`, and any `postinstall.sh` and patches beside
them. See [recipe in the KDOS glossary](../../kdos/06-reference/glossary.md#recipe).

### recipe hash

The SHA-256 over a port's recipe files and the other files in its directory that no `sha256 =` line
names, except the [*sidecar*](#sidecar). A port of KDOS's own names no source, and its recipe hash
covers all of `src/libs` as well, so an edit to any library rebuilds every such port. The build rebuilds a port when its recipe hash differs from the one
recorded at its install, and skips it otherwise. See
[recipe hash in the KDOS glossary](../../kdos/06-reference/glossary.md#recipe-hash).

### SBU

The standard build unit, the time `00_cross/binutils` took, in which this book gives every build
time. See [Build times and the SBU](../00-front/conventions.md#build-times-and-the-sbu) and
[Build times](sbu.md).

### self-hosting

Able to build itself. From `20_selfhost` on, every port is built inside the [*chroot*](#chroot),
and `20_selfhost` itself rebuilds the C library, the assembler, the linker and the compiler there,
so that no file of the toolchain every later phase uses comes out of the build container. See
[self-hosting in the KDOS glossary](../../kdos/06-reference/glossary.md#self-hosting) and
[Part V](../05-selfhost/intro.md).

### shelf

The subject directory a port of upstream software is filed under, as `cli` in
`ports/core/cli/pv/`, from the closed list in `ports/shelves`. Within a part, the pages are grouped
into one chapter per shelf. See [shelf in the KDOS glossary](../../kdos/06-reference/glossary.md#shelf).

### sidecar

A port's note for this book, the file `kfs.md` beside its recipe, with up to four sections:
*About*, *Flags*, *Patches* and *Notes*. It is not part of the [*recipe hash*](#recipe-hash), so editing it never
rebuilds the port. See
[The port's note: kfs.md](../../kdos/05-developer/writing-ports.md#the-ports-note-kfsmd).

### slot

Three things share the word. In the chapter on writing ports, a port that installs a second
version of some software beside the tree's main one, under names or a prefix of its own, as
`llvm21` beside `llvm`; see
[A second version beside the first](../../kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).
On an installed disk, one of the two root filesystems, A and B; see
[root slot in the KDOS glossary](../../kdos/06-reference/glossary.md#root-slot) and
[A/B slots and rollback](../16-wiki/ab-slots-and-rollback.md). In the desktop's drawing code, a named
colour role; see [slot in the KDOS glossary](../../kdos/06-reference/glossary.md#slot).

### snapshot

The archive of a completed phase's result under `build/snapshots/<phase>/`, which a later build
restores to continue from the phase after it. Not to be confused with the [*build-state snapshot*](#build-state-snapshot).
See [snapshot in the KDOS glossary](../../kdos/06-reference/glossary.md#snapshot) and
[Snapshots](../01-preparing/build-tree.md#snapshots-restore-points-and-timings).

### source archive

The release assets of the KDOS repository on GitHub that hold every upstream source file the
recipes name, which `make fetch` reads when the [*source cache*](#source-cache) lacks a file. See
[source archive in the KDOS glossary](../../kdos/06-reference/glossary.md#source-archive) and
[The archive](../01-preparing/sources.md#the-archive).

### source cache

`ports/.srccache`, every source file `make fetch` has verified, stored by its hash and hard-linked
into the port directories. See [source cache in the KDOS glossary](../../kdos/06-reference/glossary.md#source-cache) and
[The cache](../01-preparing/sources.md#the-cache).

### source hash

A `sha256 =` line in a [*kpkgbuild*](#kpkgbuild), naming one source file by the SHA-256 of its
contents. A copy that matches is the right file wherever it came from. Not to be confused with the
[*recipe hash*](#recipe-hash). See
[source hash in the KDOS glossary](../../kdos/06-reference/glossary.md#source-hash) and
[How a recipe names a source](../01-preparing/sources.md#how-a-recipe-names-a-source).

### staging tree

`$PKG`, the empty directory a port's `build.sh` installs into instead of `/`, which kpkg then tidies,
checks with the [*package lint*](#package-lint) and rolls into the package. See
[Anatomy of a kpkg install](../04-chroot/kpkg-install.md#build).

### step

One unit of work kdosbuild runs and logs: a numbered script of `00_cross`, `10_bootstrap` or
`70_image`, or the install of one port in a package phase. A step is what a build time is measured
for and what a failed build stops at. See [kdosbuild](../01-preparing/kdosbuild.md).

### tier

Which of three kinds of page a port gets: a full page for Tier A and Tier B, a compact card for
Tier C. See [The three tiers](../00-front/reading-a-package-page.md#the-three-tiers).

### vendor bundle

A tarball of a port's language dependencies, Rust crates, Go modules, Python distributions,
Haskell packages or npm packages, made in advance so that the build needs no network. See
[vendor bundle in the KDOS glossary](../../kdos/06-reference/glossary.md#vendor-bundle) and
[Vendor bundles](../01-preparing/sources.md#vendor-bundles).

## See also

- [The KDOS glossary](../../kdos/06-reference/glossary.md): every term of the system, defined in full
- [Typographical conventions](../00-front/conventions.md): how the book writes paths, commands and times
- [Reading a package page](../00-front/reading-a-package-page.md): the terms of a package page in place

<!-- book-nav -->
---

*Part XVII. Appendices, chapter 426.* Previous: [425. Licences](licences.md) · [Contents](../README.md)
