# Developing

This page is the entry point for contributors: what a development machine needs, how to get from a
clone to a bootable ISO, how to rebuild one small thing instead of everything, and what to run
before believing a change.

Nothing in this section assumes the rest of the book has been read. The pages it links to go
deeper: [The build system](build-system.md) explains the machinery, [Writing ports](writing-ports.md)
covers recipes, and [Testing](testing.md) covers the harnesses.

## What a development machine needs

A container runtime and disk space. That is the whole list.

The build image carries the compilers. The two host-side helpers that need toolchains of their own
— the source fetcher and the pack bake — re-execute themselves inside containers, so a clone
requires no language toolchains, no filesystem utilities, and no elevated privileges of its own.

To boot the result, add a system emulator, UEFI firmware, and hardware virtualisation.

Disk is the real cost. Budget tens of gigabytes for `build/` and about 84 GB more for a complete
set of phase snapshots, of which packaging alone is 59 GB because it carries the ISO tree.

## Getting the source

```sh
git lfs install       # before the clone, not after
git clone <this repository> kdos && cd kdos
```

Upstream tarballs live in the tree, tracked through Git LFS by the `ports/core/**` patterns in
`.gitattributes`. There is no fetch step before a build, and the build runs with no network at all.

Run `git lfs install` before cloning. Without it the working tree holds small text pointer files
where the archives should be, and the first port to unpack one fails on a corrupt archive rather
than on anything naming the cause. `git lfs pull` repairs a clone made without it.

The LFS payload is 1,020 archives, 8.27 GB, against a free allowance of 10 GiB of storage and
10 GiB a month of bandwidth, counted across every repository the account owns. Exceeding that
allowance does not slow a clone down — it blocks LFS reads outright, so a fresh clone cannot check
out at all. Keeping the repository usable therefore means a paid data pack. Only the three
`ports/core/**` patterns are tracked: `testing/fixtures/.gitattributes` opts its own archives back
out, because a fixture named `.tar.gz` is two bytes and a pointer file of a different size would
fail a layout golden with nothing naming the cause.

## The first build

```sh
make build            # compile everything — no network at all
```

`make build` builds the container image, then runs the orchestrator inside it with `--network
none`, `--privileged`, and the repository mounted: `build/` writable, everything else read-only. It
passes the calling user and group ids in so results are handed back rather than left owned by root.

The ISO lands at `build/iso-build/kdos.iso`.

Two things are worth knowing before the first run. The ISO carries no applications: nothing is
baked into it and there is no pack set to fetch, because the medium ships the catalogue and an
application is built by podman on the machine that wants it, from the store or from `kdos app
install`. An exported set imports offline. And the build refuses to overwrite an ISO a virtual
machine is reading — the emulator reads lazily, so every block the guest has not cached becomes an
I/O error the moment the file is rewritten. Shut the guest down, or override the refusal
deliberately.

## Make targets

| Target | Does | Needs |
|---|---|---|
| `all` | The default target; an alias for `build` | |
| `build` | The whole build, in the container | Container runtime |
| `fetch` | Fetch and vendor every port's sources into `ports/core`; `ports/fetch <port>` narrows it | Network, container |
| `updates` | Check every port for a newer upstream release | Network |
| `snapshots` | List the phase snapshots | |
| `run` | Boot the ISO in a virtual machine | Emulator, firmware |
| `rundisk` | Boot the disk image instead | |
| `run-hw` | Boot the ISO with hardware-accelerated graphics | Container |
| `rundisk-hw` | The same, from the disk | Container |
| `debug-boot` | Boot the kernel directly, for early-boot debugging | |
| `check-iso-free` | Refuse to rewrite an ISO in use | |
| `check-hw` | Warn about a missing accelerated-graphics setup | |
| `cleandisk` | Remove the virtual machine's disk image | |
| `cleanbuild` | Wipe `build/`, keeping snapshots and keys | |
| `clean` | Wipe `build/` including snapshots, keeping keys | |

`updates` exits non-zero by design when it finds an update, so the target tolerates status 1 and
fails only on a real error.

Arguments reach the orchestrator through `BUILD_ARGS` and the version checker through
`PORTUP_ARGS`.

## Running the result

```sh
make run          # plain graphics: the desktop works, the phosphor pass does not
make run-hw       # accelerated: the phosphor pass is on
```

`make run` uses a plain virtual display, on which the compositor falls back to software rendering.
The phosphor pass declines anything that is not the accelerated renderer, because a fullscreen
post-process on software rendering is a slideshow. `make run-hw` runs a containerised emulator with
accelerated graphics, which is the configuration where the shader is actually in the picture.

Every run target comes up at 1920x1080, and `KDOS_RES` is the single place that says so:

```sh
make run KDOS_RES=2560x1440
```

Nothing in KDOS asks for a mode. The desktop takes the connector's preferred one, which for
virtio-gpu is whatever `xres` and `yres` put in the EDID it synthesises. Left unset, those default
to QEMU's 1280x800 — about 160 by 50 characters once divided by the font's cell, which is not
enough to lay the Start menu out in the three columns it ships with. `testing/qemu-hw/run.sh` reads
the same variable and the Makefile passes it through, so the accelerated path and the plain one
cannot disagree. `testing/vnc-shot.py` takes its own `--size` and sets the same two properties.

The accelerated run asks its window to scale rather than to resize the guest, which is what makes
that setting a resolution rather than a request. QEMU's GTK window reports its own size to the
guest, virtio-gpu rebuilds the EDID around it, and the desktop takes the connector's preferred mode
— so a window opened at whatever the firmware left on screen would drag the whole desktop down with
it. `zoom-to-fit=on` scales the picture into the window instead.
`KDOS_QEMU_DISPLAY=gtk,gl=es` restores the behaviour where the guest follows the window, and
`KDOS_QEMU_DISPLAY=egl-headless` takes the window out of it entirely.

## Where the build puts things

| Path | Holds | Notes |
|---|---|---|
| `build/fs` | The target root filesystem | Root-owned by design — see below |
| `build/iso-build/kdos.iso` | The ISO | |
| `build/logs/<phase>/` | One log per step | The first thing to read when a build fails |
| `build/snapshots/<phase>/` | Phase snapshots | Survive `cleanbuild` |
| `build/keys/` | Signing keys | Survive both cleans; never committed |
| `build/fetch-home` | The source fetcher's caches and home | Written by the calling user |
| `build/podman/` | The pack bake's own container store | Root's by design |

`build/` is gitignored in its entirety.

`build/fs` is deliberately not handed back to the calling user. Changing ownership across that tree
would clear every setuid bit — the password checker, the resource helper, and both user-namespace
mapping helpers, without which no container can start at all — and would leave the account and
privilege files owned by an ordinary user. Reading it from the host needs a container or elevated
privileges, which is the correct price for a root filesystem.

## Rebuilding one thing

A full build takes hours, and almost nothing needs one. Match what changed against this table:

| Changed | Run |
|---|---|
| Something under `fs/` | `make build BUILD_ARGS="--phases 01_phase1,06_packaging --steps 01_phase1:00_file_system.sh"` |
| One port's recipe | `make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild <port>"` |
| A desktop program | `make build BUILD_ARGS="--phases 05_desktop --rebuild <port>"` |
| A library under `src/libs/` | Every port of ours — see below |
| Only packaging | `make build BUILD_ARGS="--phases 06_packaging"` |
| Nothing; resuming an interrupted run | `make build BUILD_ARGS="--continue-from 04_phase4"` |
| Everything, skipping the startup picker | `make build BUILD_ARGS=--fresh` |

Editing a library rebuilds every port of ours, not only its consumers. A recipe names the libraries
it compiles, and parsing that would mean a shell parser inside the package manager, so a
source-less port's recipe hash covers the whole of `src/libs`. Measured: about two dozen ports in a
couple of minutes. An upstream port's hash is unaffected.

The mechanics — what gets suppressed, and why forcing is passed only for the ports named — are in
[The build system](build-system.md#build-plans).

## Rules that hold while a build runs

Never edit a port's sources while its rebuild is running. The recipe hash is taken at install time,
so an edit mid-build records a hash for a tree that is not what got compiled.

Never re-run an early phase on a tree already ahead of it. Its snapshot would be overwritten with a
later tree filed under the earlier phase's name. `--continue-from` exists for exactly this.

A build started inside a backgrounded call dies with it. Start long builds so they survive the
shell that launched them.

Clear the cached host helpers when switching between a container run and a host run. The compiled
helpers under `ports/` are built against whichever C library ran last, and a binary from one cannot
execute under the other. The failure does not say so: the version checker reports that it
reproduced nothing. A container run as root also leaves those directories root-owned, so remove
them from a container rather than reaching for elevated privileges.

Do not auto-commit. Commits are the maintainer's.

## Working without a build at all

Several parts of the system run on a development host, with no container and no virtual machine:

```sh
testing/preflight.sh                   # the wiring, in seconds
testing/selftest.sh                    # libraries and consumers, about half a minute
kdosbuild --preview build 132x43 vt    # a build screen, offscreen
kdos-res --fixture … --dump            # a monitor page, offscreen
kinstall --dry-run                     # the installer, executing nothing
```

The window model runs with no display at all. `libkwm` links `libkbase` and nothing else, so
`testing/fixtures/wm/geometry.txt` replays on any host in milliseconds. It is the fastest way to
answer a window-model question — where a window lands, what a tiled state becomes, which edge a
moving edge stops against.

For iterating on a desktop program against a booted image without repacking the ISO, use
`testing/quick.sh`; see [Testing](testing.md#the-fast-loop).

## When a build fails

Work through these in order:

1. The failing step's log, under `build/logs/<phase>/`. The orchestrator names it.
2. [Build troubleshooting](build-troubleshooting.md), which catalogues the recurring failures by
   symptom. Most build failures on this tree are one of those.
3. The port's own configuration log inside the work directory, if a configuration script failed.
   `C compiler cannot create executables` almost never means what it says — the real error is on
   the failing test program.
4. `testing/preflight.sh`, which catches the dull wiring failures in seconds rather than at the end
   of a build that runs for hours.

## The phases

Eight phases run in sorted order:

| Directory | Title |
|---|---|
| `00_toolchain` | Cross Toolchain |
| `01_phase1` | Base Userland |
| `02_phase2` | Self-Hosting Bootstrap |
| `03_phase3` | Toolchain & Core Libraries |
| `04_phase4` | Userland & GUI Sliver |
| `05_desktop` | Desktop |
| `05_phase5` | Kernel |
| `06_packaging` | Packaging |

`05_desktop` sorting before `05_phase5` is deliberate rather than an accident of naming: the
desktop is ordinary userland, and the kernel is the last thing built before packaging.

What each phase contains, and how one runs, is in [The build system](build-system.md).

## See also

- [The build system](build-system.md) — phases, snapshots, build plans, the chroot
- [Writing ports](writing-ports.md) — adding or changing a recipe
- [Build troubleshooting](build-troubleshooting.md) — recurring failures, by symptom
- [Testing](testing.md) — what to run before believing a change
- [The C libraries](c-libraries.md) — what KDOS programs are built on
- [Getting started](../02-user-guide/getting-started.md) — the same build, from a user's side
