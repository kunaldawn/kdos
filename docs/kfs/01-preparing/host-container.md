# The build host and the os-dev container

Every phase of the KDOS build runs inside one [container]{.idx term="build container"} image, `os-dev`, built from the
repository's own `Dockerfile`. This chapter shows that file, explains what each package in it is
for, goes through the `docker run` line `make build` starts it with, switch by switch, and shows
how to open a shell in the same container by hand.

## Why a container, and no other host

Your machine provides Docker, the network for fetching sources, and disk space. It provides no
compiler, no library and no tool that reaches the result. The first two phases, `00_cross` and
`10_bootstrap`, compile with whatever compiler and userland they find, and they find the
container's, which are the same on every machine. Every later phase runs inside the root those two
phases built, with its own compiler. So a build on one developer's machine and a build on another's
start from identical tools, whatever distribution each runs.

There is no supported way around the container. A build on your machine's own toolchain would make
the cross compiler with your compiler, configure the bootstrap with your userland, and could link
against your libraries; none of that is tested, and the result would depend on the machine that
made it. The container also gives the build two properties it relies on: it has no network, so a
port that tries to download something fails at once instead of quietly depending on it, and it is
privileged, so the later phases can mount filesystems inside the new root and `chroot` into it.

The image is Alpine Linux, which is built on musl, the C library KDOS itself uses. The compilers
that start the bootstrap therefore already produce musl programs, and the tools `make build`
compiles for itself inside the container, kdosbuild among them, are musl programs as well. Why the
system is musl at all is in
[musl as the host C library](../../kdos/01-philosophy/decisions.md#musl-as-the-host-c-library), and
why the bootstrap builds the GNU userland rather than relying on a smaller one is in
[GNU at every name](../../kdos/01-philosophy/decisions.md#gnu-at-every-name-no-multi-call-userland).

## The image

The base image:

<!-- kfs:include Dockerfile lines=11-11 pin=ef0b31e6ca9d -->

and everything installed on top of it, with the settings the build expects:

<!-- kfs:include Dockerfile lines=14-41 pin=1e5d163c64f2 -->

### The meaning of the packages

`gcc`, `g++`, `musl-dev`, `linux-headers`
:   The compilers that build the cross toolchain in `00_cross` and kdosbuild itself, and the C
    library headers and kernel headers they compile against. Nothing they produce is installed into
    the new root: they build the cross compiler, and the cross compiler builds the root. The
    libraries GCC itself needs, GMP, MPFR and MPC, are not taken from Alpine; the cross GCC script
    unpacks them from their own ports and builds them inside GCC's source tree.

[`make`]{.idx}, [`bison`]{.idx}, [`flex`]{.idx}, [`texinfo`]{.idx}
:   What the configure scripts and makefiles of binutils, GCC and the GNU userland call while they
    build: `bison` and `flex` generate parsers, and `texinfo` provides `makeinfo` for their manuals.

`bash`, `coreutils`, `grep`, `sed`, `gawk`, `findutils`, `diffutils`, `patch`
:   The GNU userland, so that `00_cross` and `10_bootstrap` configure and build under the same tools
    upstream tests against, and that the tree's own later phases use, rather than under the BusyBox
    applets an Alpine image has by default. The build scripts are bash scripts.

`tar`, `xz`, [`zstd`]{.idx}
:   GNU tar, which unpacks the sources of `00_cross` and `10_bootstrap` and writes every
    [snapshot]{.idx}; `xz` for the many sources shipped as `.tar.xz`; `zstd`, which kdosbuild
    compresses phase snapshots with when it is present. The later phases unpack their sources with
    the tools of the root being built.

`rsync`
:   Called by the kernel's `headers_install` target, which `10_bootstrap` uses to install the Linux
    API headers into the root.

`python3`, [`git`]{.idx}, `curl`, `wget`, `nano`, `bash-doc`, `coreutils-doc`, `gcc-doc`
:   No step of `00_cross` or `10_bootstrap` names these. They make the image a usable shell for
    looking at a tree by hand. The repository's `.git` directory is never mounted into the
    container, so `git` there cannot see the checkout.

`ENV LANG=C.UTF-8`
:   The snapshot archives hold UTF-8 file names, and kdosbuild draws its build screen in UTF-8.

`WORKDIR /workspace`, `CMD`
:   The repository is mounted at `/workspace`, and a container started with no command runs the
    build. `make build` names the command explicitly.

The image is rebuilt by every `make build`, which runs `docker build -t os-dev .` first. Docker's
cache makes that instant when the `Dockerfile` has not changed. When it has changed, building the
image is the one part of `make build` that uses the network: it pulls the Alpine image and its
packages.

## How make build starts the container

This is the whole of the `make build` target:

<!-- kfs:include Makefile lines=171-199 pin=55a5d1802c0e -->

### The meaning of the switches

`check-iso-free`
:   A prerequisite target that refuses to start while `build/iso-build/kdos.iso` is held open by
    another process, normally a virtual machine booted from it, because rewriting an image under a
    running guest gives that guest I/O errors. It asks `fuser`, so on a machine without `fuser` it
    checks nothing. `make build ALLOW_ISO_IN_USE=1` overrides it.

The `case` line
:   Refuses a `KDOS_JOBS` that is not a positive whole number, before anything else runs. An empty
    one, the default, passes.

`--network none`
:   The container has no network interface but loopback. The build is a function of the sources
    `make fetch` placed beforehand, and a port whose build tries to download anything fails
    immediately. See [Offline by construction](../../kdos/01-philosophy/principles.md#offline-by-construction).

`--cpu-shares=$(KDOS_CPU_SHARES)`
:   A weight, 256 unless you set `KDOS_CPU_SHARES`, not a limit: an idle machine gives the build
    every thread, and a busy one shares the processor between the build and everything else.

`--cpus=...`
:   Present only when you set `KDOS_JOBS`. It caps the container at that many processors, because
    cargo and go size their own parallelism from the container's processor limit rather than from
    the job count. The cap is lowered to the number the machine has when that is fewer, since
    Docker refuses a larger one.

`--rm`
:   The container is deleted when the build ends. Everything the build keeps is under `build/`,
    which is on your disk.

`--privileged`
:   The package phases mount `/proc`, `/sys`, `/dev` and the repository inside `build/fs` and
    `chroot` into it, in a mount namespace of their own; a container needs these privileges to do
    so.

`-e HOST_UID`, `-e HOST_GID`
:   Your user and group. The build runs as the container's root, and when it ends,
    `script/kdosbuild.sh` gives every entry of `build/` back to you, except `build/fs`, which must
    stay root's because it is the root filesystem of the system being built. See
    [kdosbuild](kdosbuild.md).

`-e KDOS_JOBS`
:   The job count every phase uses for make, `cmake --build`, ninja and cargo. Empty, it is computed
    inside the build from the machine's threads and memory.

`-e KDOS_GIT_COMMIT`, `KDOS_GIT_DIRTY`, `KDOS_GIT_DATE`, `KDOS_GIT_TIME`
:   The current commit, whether the working tree has uncommitted changes, and the commit's date and
    time. The container cannot ask git, so they are passed in; kdosbuild records them in each
    snapshot, and the image phase stamps them into `/etc/os-release` and `/ports/TREE`.

`-e KDOS_ISO_SOURCES`, `KDOS_PACK_KDOS`, `KDOS_MAKE_BINHOST`, `KDOS_ISO_COMP`
:   Switches that change what the image phase produces. All are off or at their defaults unless you
    set them on the `make` command line; the table in
    [The first build](../../kdos/05-developer/developing.md#the-first-build) gives each.

`-e KDOS_CCACHE`, `KDOS_PKG_STORE`, `KDOS_PKG_STORE_MAX`
:   The compiler cache, on by default, and the package store, off by default. Both change how much a
    build rebuilds and nothing about what it produces; see
    [The compiler cache](../../kdos/05-developer/how-kdos-is-built.md#the-compiler-cache) and
    [The package store](../../kdos/05-developer/how-kdos-is-built.md#the-package-store).

`-v $(pwd)/build:/workspace/build`
:   The one writable mount. Everything a build makes, from the cross compiler to the ISO, is under
    `build/`.

`-v ...:ro` for `src`, `fs`, `script`, `ports`, `docs`, `testing`, the `Makefile` and the `Dockerfile`
:   The rest of the repository, read-only, so that a build cannot change its own sources. `docs` is
    there for the two ports that render the KDOS book and this one, and `testing` for the checks the
    image phase runs. The `Makefile` and the `Dockerfile` are there so that this book, rendered
    inside the build, can show the listings of this chapter, and so that a developer medium made
    with `KDOS_ISO_SOURCES=1` carries both. Nothing else of the repository is mounted: the container
    has no `.git` and none of the other files at the top of the checkout.

`$(DOCKER_TTY)`
:   `-it` when `make` itself is reading from a terminal, and nothing otherwise. Docker refuses `-it`
    without a terminal, so a build whose output is sent to a file still starts, and kdosbuild then
    prints plain lines instead of its full-screen view.

`os-dev script/kdosbuild.sh $(BUILD_ARGS)`
:   The image, and the command: compile the orchestrator if it changed and run it, with whatever you
    put in `BUILD_ARGS`. [kdosbuild](kdosbuild.md) explains both.

## A shell in the container

To look at a tree by hand with exactly the tools the build has, start the same image with the same
mounts and a shell instead of the build. From the top of the repository on your machine:

```bash
docker run --rm -it --network none --privileged \
    -v "$PWD/build:/workspace/build" \
    -v "$PWD/src:/workspace/src:ro" -v "$PWD/fs:/workspace/fs:ro" \
    -v "$PWD/script:/workspace/script:ro" -v "$PWD/ports:/workspace/ports:ro" \
    -v "$PWD/docs:/workspace/docs:ro" -v "$PWD/testing:/workspace/testing:ro" \
    -v "$PWD/Makefile:/workspace/Makefile:ro" -v "$PWD/Dockerfile:/workspace/Dockerfile:ro" \
    os-dev bash
```

You are root in `/workspace`. The image must exist: `make build` builds it, and so does
`docker build -t os-dev .` from the top of the repository. Anything you create under `build/` from
this shell is owned by root on your machine, since nothing hands it back, and is removed again from
a container rather than with `sudo`. To go on into the root being built, run
`script/chroot/exec.sh bash` from this shell;
[A shell in the chroot](../04-chroot/entering.md#a-shell-in-the-chroot) explains what that gives
you.

## See also

- [What your machine needs, and why everything runs in a container](../../kdos/05-developer/how-kdos-is-built.md#what-your-machine-needs-and-why-everything-runs-in-a-container): the same container described in the KDOS book
- [What a development machine needs](../../kdos/05-developer/developing.md#what-a-development-machine-needs): Docker, disk and the optional tools
- [Sources and the archive](sources.md): what must be fetched before the first build
- [kdosbuild](kdosbuild.md): what runs inside the container

<!-- book-nav -->
---

*Part I. Preparing, chapter 1.* Previous: [Part I. Preparing](intro.md) · [Contents](../README.md) · Next: [2. Sources and the archive](sources.md)
