# The image stage

`build/fs` holds every package the build installed, about 2,400 of them, and the next build and
the binhost are both made from it. The ISO carries a subset, the [base]{.idx term="base image"}:
the ports `ports/base` names and their dependency closure, about 1,000 packages. Every other
package is installed on a machine from the binhost of its release. So the image is not made from
`build/fs` itself but from a [stage]{.idx term="image stage"} over it, and
`script/phases/70_image/017_stage.sh` trims that stage to the base, and then trims each package
left to its run-time files. This page shows where the stage comes from, how the step decides what
to remove, and how to build an ISO that carries every package instead.

**Runs in:** the stage, `build/image-root`, an overlay over `build/fs`; not at all under
`KDOS_IMAGE=full`\
**Reads:** `ports/base`, `ports/buildonly`, the recipes and the phase lists, from `/kdos`; the
stage's package database\
**Writes:** whiteouts in `build/image-upper` for every package it removes and every development
and documentation file it strips; the stage's `/var/lib/kpkg/db/.facets/` and `/etc/kpkg.conf`;
the mark `build/image-stage.ok`

## The overlay

The stage is made by `script/chroot/exec.sh`, not by this step; [The image
stage](../04-chroot/entering.md#the-image-stage) in the chroot chapter shows the lines. Under
`KDOS_IMAGE=base`, the default, every `70_image` step from `017_stage.sh` on is chrooted into
`build/image-root`, an overlay whose lower layer is `build/fs` and whose upper layer is
`build/image-upper`. A removal, a cache rebuilt, a home written: each lands in the upper layer, and
`build/fs` is read and never written. `010_binhost.sh` and `015_pkgstore.sh` run before the stage,
on `build/fs`, so the binhost and the package store are made from every package.

This step starts the upper layer empty, so each image run trims a fresh copy of the tree as it is
now. `exec.sh` removes `build/image-stage.ok` when it starts the step, the step writes it again
when it has finished, and every later step is refused while it is missing: a later step run with
no finished `017_stage.sh` behind it stops instead of imaging an untrimmed tree.

## KDOS_IMAGE=full

<!-- kfs:include script/phases/70_image/017_stage.sh lines=47-53 pin=0d7403f37e84 -->

With `make build KDOS_IMAGE=full`, `exec.sh` mounts no overlay, every step runs on `build/fs` as
the other phases do, and this step prints the line above and exits. The ISO then carries every
installed package. It is the medium for a machine that installs with no network, and for the
rig. The `Makefile` and `exec.sh` both
refuse any value but `base` and `full`. On your machine:

```bash
make build KDOS_IMAGE=full BUILD_ARGS="--phases 70_image"
```

## The refusal

<!-- kfs:include script/phases/70_image/017_stage.sh lines=55-59 pin=c952a50d4071 -->

The step removes packages, and the same `kpkg remove` run over `build/fs` would delete most of the
build. It reads the type of the filesystem mounted at `/` from `/proc/mounts` and stops unless it
is `overlay`, which it is only when `exec.sh` made the stage.

## The removal

<!-- kfs:include script/phases/70_image/017_stage.sh lines=61-75 pin=1a05632a151c -->

[`script/lib/imageset.py`]{.idx} computes the set. `--check` reads `ports/base`, `ports/buildonly`
and `ports/groups` against the tree and stops the step on a name that is not a port, a phase or
`src` area that does not exist, or a group id that is also a port's name; preflight runs the same
check. `--drop` then prints every package the stage's database holds that is outside the set.

The set is the closure of the seeds over each recipe's first `depends =` line. `ports/base` names
its seeds three ways: port names, any number to a line; `phase <dir>`, every port that phase's
list names, which the base uses for none; and `src <area>`, every recipe under `src/<area>`, which
it uses for `art`, `desktop` and `daemons`, naming the ports of `src/system` one by one so that
`kdos-kfs`, the build book, stays off. The rest of its lines are the run time the scripts need
(`musl`, `gcc` for `libstdc++` and `libgcc_s`, `python3`, `perl`), the Plasma session as
`kde-plasma`'s list without the KDE wallpaper and Oxygen collections, the Chinese, Japanese and
Korean input engines and `fcitx5-gtk`, the applications every desk pins with their KIO workers and
thumbnailers, `firefox-esr`, the kernel and firmware, the base userland, the login, networking,
disks and filesystems, the box stack, the hardware plumbing, PipeWire, printing and scanning, the
archivers, the tools this phase runs on the stage (`squashfs-tools`, `erofs-utils`,
`libisoburn`) and the fonts. The closure does not follow a dependency into a port
[`ports/buildonly`]{.idx} lists, unless `ports/base` names that port itself: the compilers and
their bootstraps, `clang`, `libclc`, `spirv-llvm-translator` and `binutils` among them, the build
systems and generators, the headers and shader compilers a build reads, the Python build
backends, the font builders and the documentation generators.

`depends =` names build and run-time needs alike, so without that list the closure would follow
recipes into the compilers they were built with. `llvm` is not on it, because Mesa loads
`libLLVM`; `gcc` is not, because its package carries `libstdc++` and `libgcc_s`; and `clang` is,
because [mesa](#port-mesa) compiles its Intel and NVIDIA kernels with it at build time and loads
none of it, OpenCL being [mesa-opencl](#port-mesa-opencl)'s.

Every dropped name goes to one `kpkg remove`, so no removal is refused for a reverse dependency
that is going too, and kpkg's index triggers rebuild the font, icon, MIME, desktop-entry and
linker caches once, for the trimmed tree. A package still installed afterwards stops the phase.
`--sizes` logs the kept and dropped package counts and the bytes their files take; on a full
build tree it reads about 1,000 packages and 9.5 GB kept of about 2,400 and 50 GB, of which
`clang` is 2 GB.

## The facets

<!-- kfs:include script/phases/70_image/017_stage.sh lines=77-83 pin=aa724d68569a -->

What is left is the base, every package of it whole. [`kpkg facets --strip`]{.idx term="facets"}
then removes from each package the files only a compiler or a reader of manuals uses: everything
under `usr/include/`, the `.a` archives under `usr/lib/`, CMake's package files, pkg-config and
aclocal files, the GObject introspection XML and Vala's API files, the `-l` links
(`libz.so -> libz.so.1.3.1`), man sections 0, 2 and 3, and `usr/share/doc/` (less KDE's handbooks
and the two books), `info` and `gtk-doc`. The rules, and the exceptions that keep a running
program whole, are in [Facets](../../kdos/03-architecture/packaging.md#facets). Each package's
manifest and file record are rewritten without those files, and each file is listed in
`/var/lib/kpkg/db/.facets/<name>`, so `kpkg check` passes the stripped tree. Every package prints
a line, and the last line says how much went; on a full build tree it is about 0.95 GB of the
base's 5.5 GB, 270 MB of it gcc's compiler, which gcc's recipe moves into the development facet
with its `facet_dev =` line.

The stage's `/etc/kpkg.conf` then says `FACETS="${KPKG_FACETS:-run}"`, so every later install on
the medium, and on a machine installed from it, leaves the same files out. `build/fs` keeps
`FACETS=all` and every file, which is what the next build compiles against. On a machine:

```bash
kpkg facets --list zlib                  # what zlib was installed without
sudo kdos update install --dev zlib      # zlib again, whole, from the binhost
```

## What proves the base whole

Nothing in this step checks that the base runs. Every later step reads the trimmed stage, and
[087_gates.sh](087-gates.md) is the proof: its `linkcheck.py` and `entrycheck.py` fail the image
when a library a program links, an interpreter a script names or a program a desktop entry runs
resolves to a package that went, or to a `-l` link the facets took. A port that a base program needs at run time and that the
closure missed, usually a dependency a recipe does not declare or a port wrongly listed in
`ports/buildonly`, stops the build there. A QML module, a Python package or a port loaded by
name is seen by neither check; `imageset.py --verify`, the gate's fourth check, reads the run-time
needs each package's install recorded and fails a kept package whose QML import, Python import or
`runtime =` port only a removed package provides
([Run-time needs](../../kdos/03-architecture/packaging.md#run-time-needs)).

To put a port on the medium, add it to `ports/base` and run the image phase again:

```bash
make build BUILD_ARGS="--phases 70_image"
```

## What it leaves

On your machine, `build/image-upper` and `build/image-work`, the overlay's layers, and
`build/image-root`, its mount point, which is empty outside a step. They belong to root, like
`build/fs`, and kdosbuild does not give them back to you when the build ends. `build/fs` is
unchanged. The ISO, written by [110_iso.sh](110-iso.md), is the stage.

## See also

- [The base image](../../kdos/03-architecture/packaging.md#the-base-image): the set, the three files and why a base ISO and its binhost are one unit
- [Installing software](../16-wiki/updating.md#installing-software): how a machine adds what the base leaves out
- [Facets](../../kdos/03-architecture/packaging.md#facets): the rules `kpkg facets --strip` applies, and the restore
- [The image gates](087-gates.md): the proof that the base closure is whole
- [Entering the chroot](../04-chroot/entering.md#the-image-stage): the overlay's lines in `exec.sh`

<!-- book-nav -->
---

*Part XV. The image, chapter 366.* Previous: [365. The package store](015-pkgstore.md) · [Contents](../README.md) · Next: [367. Cleaning up the root](020-cleanup.md)
