# The cross toolchain

<!-- kfs:phase 00_cross -->
<!-- kfs:order binutils gcc -->

The first phase, `00_cross`, builds a compiler that runs in the build container and produces
programs for KDOS. It is two scripts: binutils, then a first GCC with only the C and C++ front
ends. Both install under `build/cross`, beside the root rather than inside it, and nothing they
install is shipped. kpkg does not exist yet, so neither stage is a port. Each is a plain shell
script, and the two chapters of this part show its commands as the tree has them and explain every
switch. This page covers what the toolchain is for, the target name it is built for, and the
environment both scripts run in.

The whole part takes about 8.8 [SBU]{.idx}: 1 SBU for binutils, by definition, and 7.8 for GCC.

## Why a cross compiler

The container already has a compiler, Alpine's. It is a musl compiler for x86-64, the same
architecture and C library KDOS uses, and it would turn out working programs. They would still be
the container's programs: compiled against the container's headers in `/usr/include`, linked
against the container's libraries in `/usr/lib`, and configured by scripts that probed the
container to decide what the system offers. Copied into KDOS, each would carry those assumptions
with it.

A [cross toolchain]{.idx} closes that door. It is built for a target that names a system other than the
one it runs on, and it is given `build/fs` as its system root, so every header it reads and every
library it links is one this build put there. A configure script told
`--host=x86_64-kdos-linux-musl` calls the target's prefixed tools, `x86_64-kdos-linux-musl-gcc` and
the rest. When it is also told a `--build` that names the container, the two differ and it knows it
is cross-compiling, so it does not run the test programs it compiles, which would run in the
container and report on the container. Told `--host` alone, it decides by running one such program,
and because the container runs x86-64 musl programs, it concludes it is not cross-compiling;
[The shape of a step](../03-bootstrap/intro.md#the-shape-of-a-step) in Part III says which steps
that is and what it means for them.
[How KDOS is built](../../kdos/05-developer/how-kdos-is-built.md#the-cross-toolchain-00_cross)
tells the same reasoning as part of the whole build.

## Two target names

A target [triplet]{.idx term="target triplet"} is architecture, vendor and system: `x86_64`, `kdos`, `linux-musl`. KDOS uses two.

**`x86_64-kdos-linux-musl`** is the triplet of the build. `script/env/host.env` sets it as
`KDOS_TARGET`, and every compiler that `00_cross` and `10_bootstrap` build is configured for it:
the cross GCC under `build/cross`, and the native GCC that `10_bootstrap` builds into `build/fs`.
Its vendor field, `kdos`, means nothing beyond being different from the container's own triplet, so
that a build for it is always a cross build and never mistaken for a native one.

**`x86_64-pc-linux-musl`** is the triplet of the compiler KDOS ships. From `20_selfhost` on, the
`gcc` port is configured with no triplet at all, so its configure asks `config.guess`, which names
the machine `x86_64-pc-linux-musl`. Its files go to `/usr/lib/gcc/x86_64-pc-linux-musl` and the
other paths named after that triplet. The bootstrap compiler's files carry the other name, so the
native compiler overwrites none of them, and the `gcc` port's install hook deletes them once it is
in place (see [Writing ports](../../kdos/05-developer/writing-ports.md#postinstallsh)).

## The environment

Both phases of the container half, `00_cross` and `10_bootstrap`, source the same file through
their `phase.env`:

<!-- kfs:include script/env/host.env lines=18-41 pin=2b257ddf3fa1 -->

**`KDOS_TARGET`** is the triplet above.

**`WORKSPACE`** is where the tree is. The container mounts the repository at `/workspace`; `kdos
rebuild` builds from a copy elsewhere and names it in `KDOS_WORKSPACE`. Every path below hangs from
it, so this book writes them relative to the tree: `build/fs` means `/workspace/build/fs` inside the
container.

**`SYSROOT`** is `build/fs`, the root being built, and **`CROSS_SYSROOT`** is `build/cross`, where
the cross toolchain is installed. Despite its name, `CROSS_SYSROOT` is not a system root: it is the
toolchain's `--prefix`. The toolchain's system root is `SYSROOT`.

**`build/tmp`** is emptied every time the file is sourced. Every step sources it, so every step
begins with an empty `build/tmp`, which is where it unpacks its sources.

**`PKG_CONFIG_PATH`, `PKG_CONFIG_LIBDIR` and `PKG_CONFIG_SYSROOT_DIR`** make `pkg-config` see only
the `.pc` files under `build/fs` and prefix every path it prints with `build/fs`. A configure that
asks for a library finds the one being built for KDOS, or none, never the container's.

**`PATH`** puts `build/cross/bin` first, so each prefixed tool is found as soon as it is installed.

**`CFLAGS`, `CXXFLAGS` and `LDFLAGS`** are the plain flags of both container phases: `-O2 -pipe`,
with C held to `-std=gnu99`. A standard older than GCC's own default matters wherever GCC builds
its runtime libraries; see
[Build troubleshooting](../../kdos/05-developer/build-troubleshooting.md#a-language-standard-reaching-the-compilers-own-runtime).
The hardening flags of the package phases (see
[the release flags](../../kdos/05-developer/writing-ports.md#the-release-flags)) are not here. kpkg
and kinstall, which no later phase rebuilds, carry them on their own compile lines.

The file ends by sourcing `script/env/common.env`, which every phase shares. It appends
`-ffile-prefix-map` to the compiler flags and `-Wl,--build-id=sha1` to the link flags, pins
`SOURCE_DATE_EPOCH`, `TZ=UTC` and `LC_ALL=C`, and sets the job count: `KDOS_JOBS`, or the thread
count clamped to one job per 2 GiB of memory, exported as `MAKEFLAGS=-jN`. The plain `make` in
every script of this part and the next therefore runs in parallel without saying so. What those
lines are for is in
[Reproducible packages](../../kdos/03-architecture/packaging.md#reproducible-packages).

`00_cross/phase.env` then sets one variable of its own:

<!-- kfs:include script/phases/00_cross/phase.env lines=25-27 pin=9e3304f6f6b7 -->

`MARK` is `build/mark/cross`. Each script ends by touching a file there and begins by exiting when
that file exists. [Part III](../03-bootstrap/intro.md) explains the guard, which every early step
shares.

## Running the phase

`make build` runs the phase as part of the whole build. On a tree that has been through it already,
each script stops at its guard at once. To run one script again, name it in a build plan:

```sh
make build BUILD_ARGS="--phases 00_cross --steps 00_cross:00_binutils.sh"
```

A step named in a plan runs with `KDOS_REPLAY=1`, which stands its guard down. See
[Build plans](../../kdos/05-developer/build-system.md#build-plans) for what else a plan changes.

## See also

- [Part III](../03-bootstrap/intro.md): what this compiler builds
- [How KDOS is built](../../kdos/05-developer/how-kdos-is-built.md#the-cross-toolchain-00_cross): the cross toolchain in the whole journey
- [The build system](../../kdos/05-developer/build-system.md#the-phase-environment): how a phase's environment is assembled

<!-- book-nav -->
---

*Part II. The cross toolchain.* Previous: [5. kdosbuild](../01-preparing/kdosbuild.md) · [Contents](../README.md) · Next: [6. Cross binutils](binutils.md)
