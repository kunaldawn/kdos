# The build tree

Everything a build makes goes under one directory, `build/` at the top of the repository, which is
the only part of the repository the build container may write. This chapter goes through what is
in it: the root being built, the cross toolchain, the markers and logs, the [snapshots]{.idx term="snapshot"} and caches,
and the keys. It ends with what the two clean targets remove and how to start again from nothing.

## The layout

A tree built as far as the middle of `41_system` holds this much at its top:

```text
build/
├── .devplan.json      the build plan in force
├── .kdosbuild         the compiled orchestrator
├── .kdosbuild.sum     the hash of what it was compiled from
├── .snap-lineage/     the index each next snapshot is compared against
├── ccache/            the compiler cache
├── cross/             the cross toolchain
├── fs/                the root filesystem being built
├── logs/              one log per step
├── mark/              the early phases' "already done" markers
├── snapshots/         phase snapshots and the build's timings
└── tmp/               scratch space for the early phases
```

Later phases and opt-in settings add more: `iso-build/` with the finished `kdos.iso`, `keys/`,
`pkgstore/`, `image-ports/`, `kdos-base/`, `binhost/`, and `kfs/`, where `make book` renders this
book. The full table, with every file and who writes it, is in
[Where the build puts things](../../kdos/05-developer/developing.md#where-the-build-puts-things).

### fs: the root being built

`build/fs` is the target tree, the root filesystem of the system being built. `10_bootstrap`
creates it, every later phase installs into it, and the image phase turns it into the ISO's
`system.sfs`. It is also the chroot every phase from `20_selfhost` on runs inside.

It belongs to root on your machine and stays that way. When a build ends, `script/kdosbuild.sh`
hands every other entry of `build/` back to your user, but not this one: changing the owner of a
file clears its setuid bit and rewrites its owner, and the root's setuid programs and its
`/etc/shadow` must ship exactly as the packages installed them. Reading or removing `build/fs`
therefore needs root, or a container such as the one in
[A shell in the container](host-container.md#a-shell-in-the-container).

Its package database, `build/fs/var/lib/kpkg/db`, is the record of what is installed, which recipe
each package came from, and which files it owns.
[Anatomy of a kpkg install](../04-chroot/kpkg-install.md) shows one entry.

### cross: the cross toolchain

`build/cross` holds the binutils and GCC that `00_cross` builds, which run in the container and
produce programs for `x86_64-kdos-linux-musl`. `10_bootstrap` compiles the first root with them.
Nothing in `build/cross` is installed into the root.

### mark: what the early phases have done

Every script of `00_cross` and `10_bootstrap` except the first step of `10_bootstrap` ends by
creating a marker file, and starts by exiting at once when its marker is already there. The markers
are under `build/mark/cross/` and `build/mark/bootstrap/`, one empty file per step, named after
what the step built:

```text
build/mark/cross:      binutils  gcc
build/mark/bootstrap:  bash  binutils  coreutils  diffutils  findutils  gawk  gcc  grep  gzip
                       kinstall  kpkg  libstdc++  linux_headers  make  musl_libc  ncurses  patch
                       readline  sed  tar  xz
```

The guard at the top of `00_cross/00_binutils.sh` is typical:

<!-- kfs:include script/phases/00_cross/00_binutils.sh lines=17-19 pin=25be1f575482 -->

This is why a second run of the two early phases costs seconds. A marker stands down when kdosbuild
exports `KDOS_REPLAY=1`, which it does for a step a build plan names explicitly, so a named step
runs again whatever its marker says. The marker for `kpkg` is the one exception to "empty": it
holds a hash of every source file kpkg is compiled from, so that a change to kpkg or its libraries
rebuilds it. See
[Guards in the early phases](../../kdos/05-developer/build-system.md#guards-in-the-early-phases).

### logs: one file per step

Every step writes all its output, verbatim, to its own file:

```text
build/logs/<phase>/<NNNN>_<name>.log
```

`NNNN` is the step's position within its phase, counted from zero. A script step is named after its
script, less the number in front, and a package step after its port with `.install` added:

```text
build/logs/00_cross/0000_binutils.sh.log
build/logs/10_bootstrap/0000_file_system.sh.log
build/logs/41_system/0145_pv.install.log
```

When `--port-jobs` builds a phase level by level, each level's install step has a log of its own,
`<NNNN>_L<level>.commit.log`. Three more files sit directly under `build/logs/`:
`snapshots.log`, every notice the build shows, with the time; `chroot.log`, warnings from the
chroot wrapper; and, under a phase whose package list could not be resolved, `expansion.log`
saying why. When a step fails, its log is the first thing to read; see
[Build troubleshooting](../../kdos/05-developer/build-troubleshooting.md).

### snapshots: restore points and timings

When a run is asked to write snapshots, each phase from `00_cross` to `50_desktop` is archived as it
completes into `build/snapshots/<phase>/`, so that a later build can start from it instead of from
the beginning. A snapshot directory holds one compressed archive per path the phase declares and a
`manifest.json` describing it:

```text
build/snapshots/30_foundation/
├── fs.tar.zst
└── manifest.json
```

The first snapshot of a path holds the whole tree; each later one is a layer holding only what
changed since the one before, compared against the index in `build/.snap-lineage/`. A snapshot
other snapshots still layer on is moved to `build/snapshots/.held/` when it is replaced, until
nothing needs it. Writing snapshots is off unless a run asks for it.
[kdosbuild](kdosbuild.md) shows how to ask, and how to restore one.

The same directory holds `timings.json`, the seconds each step took the last time it did its work
and the machine each was measured on, which the book's build times are computed from; see
[Typographical conventions](../00-front/conventions.md#build-times-and-the-sbu). Like the
snapshots, it survives `make cleanbuild`.

### Caches and the package store

`build/ccache` is the compiler cache that CMake ports in the chroot compile through, up to 20 GB.
`build/pkgstore` holds every package the chroot phases build when `make build KDOS_PKG_STORE=1` is
given, so that a later build can install a port whose inputs have not changed instead of building
it again. Both make a build faster and change nothing about what it produces; see
[The compiler cache](../../kdos/05-developer/how-kdos-is-built.md#the-compiler-cache) and
[The package store](../../kdos/05-developer/how-kdos-is-built.md#the-package-store).

### keys: signing keys

`build/keys` holds keys, not build output: the kernel's [module-signing]{.idx term="module-signing key"} key, which the `linux` port
writes there the first time it builds without one, and the release-signing key when a maintainer
has made one. It survives both clean targets. Losing the module-signing key means the next kernel
carries a new certificate, so modules from earlier builds load only as tainted under it.

### tmp and the rest

`build/tmp` is scratch space for `00_cross` and `10_bootstrap`, emptied at the start of each of
their steps. `build/.kdosbuild` is the orchestrator, compiled from source at the start of every
`make build` when its sources, flags or compiler differ from the hash in `build/.kdosbuild.sum`.
`build/.devplan.json` is the build plan in force, which the plan picker opens on.

## Cleaning

Two targets empty `build/`, and they differ in what they keep:

<!-- kfs:include Makefile lines=244-266 pin=5da386d0c722 -->

`make cleanbuild` keeps the snapshots, the compiler cache, the package store and the keys, all of
which stay valid for the next build. `make clean` keeps only the keys.

Both run on your machine as your user, and `build/fs` belongs to root, so neither can remove it:
the `find` fails on it, the target ignores the failure, and the old root is still there for the
next build to run on. Remove it from a container:

```bash
docker run --rm -v "$PWD/build:/b" os-dev rm -rf /b/fs
```

## Starting from nothing

A build does not empty the tree it finds. Every early step whose marker exists exits at once, and
every port installed from an unchanged recipe is skipped, which is what makes a second build cheap.
To build from nothing, remove the three directories that record that progress, from a container,
and run every phase:

```bash
docker run --rm -v "$PWD/build:/b" os-dev rm -rf /b/fs /b/mark /b/cross
make build BUILD_ARGS=--fresh
```

The snapshots, caches and keys are left alone. Budget most of a day for the build that follows; see
[Building from scratch](../../kdos/05-developer/developing.md#building-from-scratch).

## See also

- [Where the build puts things](../../kdos/05-developer/developing.md#where-the-build-puts-things): every path under `build/`, and the helpers under `ports/`
- [Snapshots](../../kdos/05-developer/build-system.md#snapshots): layers, the index and what a restore does
- [Step logs](../../kdos/05-developer/build-system.md#step-logs): the log names and the files beside them
- [kdosbuild](kdosbuild.md): the program that writes most of this tree

<!-- book-nav -->
---

*Part I. Preparing, chapter 4.* Previous: [3. The fs/ overlay](fs-overlay.md) · [Contents](../README.md) · Next: [5. kdosbuild](kdosbuild.md)
