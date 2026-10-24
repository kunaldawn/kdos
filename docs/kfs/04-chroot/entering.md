# Entering the chroot

Every phase from `20_selfhost` on runs inside `build/fs`, the root the bootstrap built, entered as
a [chroot]{.idx}. kdosbuild enters it once for every command it runs there, through one script,
`script/chroot/exec.sh`. This chapter goes through that script part by part: the mount [namespace]{.idx}
it takes, the filesystems and repository directories it mounts, the limit it raises, and the empty
environment it starts the command in. It then shows how to open a shell in the chroot by hand, with
the same script or with its interactive counterpart, `script/chroot/enter.sh`.

## Why a chroot

`chroot` makes a directory the root directory for the command it starts. Inside, `/usr/bin/gcc` is
`build/fs/usr/bin/gcc`, the compiler the bootstrap built, and the container's own compiler,
libraries and tools are out of reach. From `20_selfhost` on, a package is therefore built only by
what the tree itself holds. The empty environment the script starts each command in does the same
for variables: nothing set in the container reaches a build unless the script names it.

## script/chroot/exec.sh

kdosbuild runs every command of a chroot phase as `script/chroot/exec.sh <command>`, from the top of
the repository in the container. A package step's command is

```bash
script/chroot/exec.sh bash -c 'source script/phases/41_system/phase.env && kpkg install pv'
```

and a script step's is `script/chroot/exec.sh bash script/phases/70_image/<script>`.

### A private mount namespace

<!-- kfs:include script/chroot/exec.sh lines=13-29 pin=5922b403a114 -->

The script requires root, and then runs itself again under `unshare --mount --propagation private`,
which gives the new copy a mount namespace of its own: every mount it makes from here on is visible
only to it and to the processes it starts. `KDOS_CHROOT_NS` tells the second copy that it is the
second copy. When the command's last process exits, the namespace and every mount in it disappear,
even when the command is killed, so there is nothing to unmount afterwards and nothing a stopped
build can leave mounted. Two commands running at once, as two ports do under `--port-jobs`, each
have their own `/proc`, `/tmp` and repository mounts and never see the other's.

### Finding the tree

<!-- kfs:include script/chroot/exec.sh lines=31-49 pin=dfb38997d9bc -->

The repository root is found from the script's own path, two directories up, and the tree is
`build/fs` under it; the script stops if the tree does not exist. Warnings go to
`build/logs/chroot.log` and never to the command's output, because kdosbuild reads the output of
`kpkgdepends` run through this script as a list of package names, and a stray line there would be
taken for one.

### The home directory

<!-- kfs:include script/chroot/exec.sh lines=51-59 pin=dd60329fe977 -->

The mount points are created when missing. The build's `HOME` is `/var/tmp/kdos-home` rather than
`/root`, so that every tool's dotfiles, caches and package registries land under `/var/tmp`, which
no snapshot archives and the image does not ship. It is on disk rather than in the per-command
`/tmp`, because a Go or Rust build keeps gigabytes in its cache there.

### Virtual filesystems

<!-- kfs:include script/chroot/exec.sh lines=61-79 pin=0abb9df868ed -->

`/dev` is the container's, bound in. `/proc` and `/sys` are mounted fresh, so that inside the chroot
they describe the processes of this namespace. The container's cgroup tree is bound read-only over
the fresh `/sys/fs/cgroup`, because cargo and go read their processor limit from it, and would
otherwise see every thread of the machine whatever `--cpus` cap the container has. `/tmp` and `/run`
are new, empty `tmpfs` filesystems for every command, so nothing one step leaves there survives into
the next.

### The repository

<!-- kfs:include script/chroot/exec.sh lines=81-124 pin=badafa5a1317 -->

The repository is bound at `/kdos` and `build/` at `/kdos/build`, writable, and `ports/` at
`/ports`, the path every phase's `PORT_REPO` and kpkg's own configuration name. A bind mount of a
directory does not carry the mounts beneath it, and in the container `script/`, `src/`, `fs/`,
`docs/`, `testing/`, the `Makefile` and the `Dockerfile` are separate read-only mounts, so each is
bound again explicitly. `docs/`, `testing/` and the two files are remounted read-only. `docs/` is
read by the two ports that render the books, `kdos-doc` for the KDOS book and `kdos-kfs` for this
one; `testing/`, with `ports/` once more at `/kdos/ports`, by the checks the image phase runs from
`/kdos`; and the `Makefile` and `Dockerfile` by `kdos-kfs` again, for the listings of
[Part I](../01-preparing/host-container.md), and by the image step that copies the sources onto a
developer medium.

### The open-files limit

<!-- kfs:include script/chroot/exec.sh lines=126-137 pin=5442fd7ffe9b -->

Both the hard and the soft limit on open files are raised to at least 4096. QtWebEngine's link runs
`ulimit -n 4096` itself and fails when the hard limit is lower. A limit already that high is left
alone, and one that cannot be raised is noted in `build/logs/chroot.log`.

### The package store

<!-- kfs:include script/chroot/exec.sh lines=139-143 pin=899f3439074f -->

`make build KDOS_PKG_STORE=1` or `=check` turns on the [package store]{.idx}, and the script turns that into
the two variables kpkg itself reads: the store's directory as the chroot sees it, and whether to
build every hit anyway and compare.

### The command, in an empty environment

<!-- kfs:include script/chroot/exec.sh lines=192-213 pin=1e0107faac8d -->

`chroot` makes `build/fs` the root and runs the tree's own `/usr/bin/env -i`, which clears the
environment completely and sets only the variables listed. Then the tree's bash changes to `/kdos`,
so that a relative path such as `script/phases/41_system/phase.env` means the same file inside as
outside, and replaces itself with the command. In `bash -c '...' -- "$@"`, the `--` becomes the
shell's `$0` and the command and its arguments become `"$@"`.

The variables that cross into the chroot:

`HOME`, `TERM`
:   The build's home directory, above, and the caller's terminal type.

`PATH`
:   `/usr/bin` first, then `/usr/sbin`, `/bin`, `/sbin`, and `/usr/local/bin` last. `/usr/bin` comes
    before `/bin`, which is a link to it, because CMake turns each entry into a search prefix, and a
    package's CMake configuration found through `/bin` computes its prefix as `/`. `/usr/local/bin`,
    where KDOS's own tools such as `kdos-appbox` install, is last so that the image steps that call
    them by name find them, while none of them can shadow a system tool during a port's configure.

`KDOS_REPLAY`
:   `1` for a step a build plan named, so that its "already done" marker stands down.

`KDOS_JOBS`
:   The job count from `make build`, or empty, in which case the phase environment computes it
    inside the chroot.

`KDOS_ISO_SOURCES`, `KDOS_PACK_KDOS`, `KDOS_MAKE_BINHOST`, `KDOS_ISO_COMP`, `KPKG_KEEP_CACHE`
:   The opt-in switches of the image phase, and `KDOS_MAKE_BINHOST` again under the name kpkg reads,
    so that every package is kept for the binhost.

`KDOS_CCACHE`
:   Whether CMake ports compile through the cache in `build/ccache`.

`KDOS_PKG_STORE`, `KDOS_PKG_STORE_MAX`, `KPKG_STORE`, `KPKG_STORE_CHECK`, `KPKG_STORE_SALT`
:   The package store, as above, its size cap, and the hash of the bootstrap phases that kdosbuild
    computes on the host and every store key includes.

`KDOS_GIT_COMMIT`, `KDOS_GIT_DIRTY`, `KDOS_GIT_DATE`, `KDOS_GIT_TIME`
:   The commit the tree is being built from, stamped into the image.

A variable the `Makefile` passes into the container but this line does not name reaches the two
container phases and none of the chroot ones. Adding a build setting is therefore two edits: the
`-e` in the `Makefile` and the name on this line. Every variable is described in
[`env -i` means every variable must be named](../../kdos/05-developer/build-system.md#env--i-means-every-variable-must-be-named).

## The phase environment

The environment `env -i` leaves is not the one a port is built with. A package step's command
starts with `source script/phases/<phase>/phase.env`, and that file, through
`script/env/chroot.env` and `script/env/common.env`, sets everything the build needs: the compilers
named outright, the release flags, the job count, the reproducibility settings, the repositories
kpkg looks in. Part of what it gives a `41_system` step, printed with `env` in a container whose job
count came to 16, with the flag values cut short:

```text
CC=gcc
CXX=g++
CFLAGS='-O2 -pipe -std=gnu11 -fPIC -fno-semantic-interposition -fstack-clash-protection ...
KDOS_JOBS=16
MAKEFLAGS=-j16
KPKG_STRICT_RECIPE=1
LANG=C.UTF-8
PATH=/kdos/script/bin:/usr/bin:/usr/sbin:/bin:/sbin:/usr/local/bin
PORT_REPO='/ports/core /kdos/src/system /kdos/src/art'
SOURCE_DATE_EPOCH=1735689600
TZ=UTC
```

`/kdos/script/bin`, put first once ninja is installed, holds wrappers that add the job count to a
`ninja` call and `--wrap-mode=nodownload` to a `meson setup`; it is not part of the image.
`KPKG_STRICT_RECIPE=1` makes kpkg rebuild a port whose recipe changed since it was installed.
`SOURCE_DATE_EPOCH` and `TZ` fix every timestamp a build writes. The full list of what each file
sets is in [The phase environment](../../kdos/05-developer/build-system.md#the-phase-environment).

## A shell in the chroot

To work in the chroot by hand with exactly what a build step has, run `exec.sh` with a shell as its
command. From the top of the repository on your machine, start the build container with the same
mounts `make build` gives it and `exec.sh` inside it:

```bash
docker run --rm -it --network none --privileged \
    -v "$PWD/build:/workspace/build" \
    -v "$PWD/src:/workspace/src:ro" -v "$PWD/fs:/workspace/fs:ro" \
    -v "$PWD/script:/workspace/script:ro" -v "$PWD/ports:/workspace/ports:ro" \
    -v "$PWD/docs:/workspace/docs:ro" -v "$PWD/testing:/workspace/testing:ro" \
    -v "$PWD/Makefile:/workspace/Makefile:ro" -v "$PWD/Dockerfile:/workspace/Dockerfile:ro" \
    os-dev script/chroot/exec.sh bash
```

You are root in `/kdos`, inside `build/fs`, with the empty environment above. Load the phase
environment of the port you are working on, and kpkg behaves exactly as it does in the build:

```bash
source script/phases/41_system/phase.env
kpkg info pv
```

```text
Package: pv
Version: 1.12.0-1
License: GPL-3.0-or-later
Files: 16
```

`Files` counts every path the package's database entry lists, directories included. Leave the shell
with `exit`; its mounts go with it.

A command can be given in place of the shell, as kdosbuild gives one. To see the install order
`kpkgdepends` resolves for `pv`, with the database hidden so that installed ports are not dropped
from it, as kdosbuild resolves a phase:

```bash
script/chroot/exec.sh bash -c 'source script/phases/41_system/phase.env && PKGDB_DIR=/dev/null kpkgdepends pv'
```

```text
ncurses pv
```

Without `PKGDB_DIR=/dev/null` the same command prints an empty line on a tree where both are
installed and current, because the solver leaves out every port with nothing to do.

## script/chroot/enter.sh

`enter.sh` is a lighter way into a tree, for looking around rather than building:

<!-- kfs:include script/chroot/enter.sh lines=13-61 pin=04cfa2a9892d -->

It takes a private mount namespace in the same way and mounts `/dev`, `/proc`, `/sys`, `/tmp` and
`/run`, but it differs from `exec.sh` in three ways. It does not mount the repository, so there is
no `/kdos` and no `/ports` inside, and kpkg cannot find a port. It does not clear the environment,
so the shell inherits whatever the caller had. And it copies the caller's `/etc/resolv.conf` into the
tree, over the copy `fs/` provides, so that name resolution works from a chroot on a machine with a
network; the next run of `000_file_system.sh` puts `fs/`'s copy back. With no arguments it starts a
login shell, `bash -l`, which reads the tree's own `/etc/profile`; with arguments it runs them
instead.

It needs only root, `unshare`, `mount` and `chroot`. In the build container it runs the same way as
`exec.sh` above, with `script/chroot/enter.sh` as the command. The build never uses it.

## See also

- [Anatomy of a kpkg install](kpkg-install.md): what the command inside the chroot then does
- [The chroot](../../kdos/05-developer/build-system.md#the-chroot): the same script in the KDOS book
- [Entering the chroot](../../kdos/05-developer/how-kdos-is-built.md#entering-the-chroot): where the chroot fits in the whole build
- [The os-dev container](../01-preparing/host-container.md): the container the chroot is entered from

<!-- book-nav -->
---

*Part IV. The chroot and kpkg, chapter 31.* Previous: [Part IV. The chroot and kpkg](intro.md) · [Contents](../README.md) · Next: [32. Anatomy of a kpkg install](kpkg-install.md)
