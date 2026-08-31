# Developing

Getting from a clone to a built ISO, and then iterating without rebuilding everything. This is the
entry point for the developer guide: what to install, what every target does, and the narrow loops
that turn an hours-long build into a few minutes.

## What you need

**A container runtime and disk.** Nothing else is installed on your machine.

The build image carries the compilers, and the two host-side helpers that need a toolchain of
their own — the source fetcher and the pack bake — **re-execute themselves inside containers**, so
a clone needs no language toolchains, no filesystem tools and no elevated privileges of its own.

To run the result you also want a system emulator, UEFI firmware, and hardware virtualisation.

## The first build

```sh
make bootstrap        # fetch upstream sources — needs network, once
make build            # compile everything — no network at all
```

`make build` builds the container image, then runs the orchestrator inside it with
`--network none`, `--privileged`, and the repository mounted: `build/` writable, everything else
read-only. It passes your user and group ids in so the results are handed back to you.

The ISO lands at **`build/iso-build/kdos.iso`**.

Two things to know before the first run:

- **The application packs are separate.** `make bootstrap-packs` downloads a baked set;
  `make fetch-packs` bakes one yourself. Without either you get a working ISO with no application
  catalogue.
- **The build refuses to overwrite an ISO a virtual machine is reading.** Rewriting it while a
  guest boots from it corrupts that guest — the emulator reads lazily, so every block the guest has
  not cached becomes an I/O error. Shut the guest down, or override deliberately.

## Every target

| Target | Does | Needs |
|---|---|---|
| `all` | The default target: an alias for `build` | |
| `build` | The whole build, in the container | Container runtime |
| `bootstrap` | Fetch upstream sources into `ports/core` | Network |
| `bootstrap-packs` | Download a baked pack set | Network |
| `fetch` | Fetch and vendor sources directly | Network, container |
| `fetch-packs` | Bake the pack set yourself | Network, container, about an hour |
| `publish-sources` | Upload new source archives | A token |
| `publish-packs` | Upload a baked pack set | A token |
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
| `cleanbuild` | Wipe `build/` but **keep snapshots** and keys | |
| `clean` | Wipe `build/` including snapshots, keeping keys | |

**`updates` exits non-zero by design when it finds an update**, so the target tolerates that and
fails only on a real error.

## Running it

```sh
make run          # plain graphics: the desktop works, the phosphor pass does NOT
make run-hw       # accelerated: the phosphor pass is on
```

`make run` uses a plain virtual display, where the compositor falls back to software rendering —
and the phosphor pass declines anything that is not the accelerated renderer, because a fullscreen
post-process on software rendering is a slideshow. `make run-hw` runs a containerised emulator with
accelerated graphics, which is the configuration where the shader is actually in the picture.

## Where things land

| Path | What | Notes |
|---|---|---|
| `build/fs` | The target root filesystem | **Root-owned by design** — see below |
| `build/iso-build/kdos.iso` | The ISO | |
| `build/logs/<phase>/` | One log per step | The first thing to read when a build fails |
| `build/snapshots/<phase>/` | Phase snapshots | Survive `cleanbuild` |
| `build/keys/` | Signing keys | Survive both cleans; **never committed** |
| `build/podman/` | The pack bake's own container store | Root's by design |

**`build/` is gitignored in its entirety.**

**`build/fs` is deliberately not handed back to your user.** Changing ownership across that tree
would clear every setuid bit — the password checker, the resource helper, and both user-namespace
mapping helpers, without which no container can start at all — and would leave the account and
privilege files owned by an ordinary user. Reading it from your host needs a container or elevated
privileges, which is the correct price for a root filesystem.

## Iteration loops

A full build is hours. Almost nothing needs one. Find what you changed:

| You changed | Run |
|---|---|
| Something under `fs/` | `make build BUILD_ARGS="--phases 01_phase1,06_packaging --steps 01_phase1:00_file_system.sh"` |
| One port's recipe | `make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild <port>"` |
| A desktop program | `make build BUILD_ARGS="--phases 05_desktop --rebuild <port>"` |
| A library under `src/libs/` | Rebuild every consumer — see below |
| Only packaging | `make build BUILD_ARGS="--phases 06_packaging"` |
| Nothing; you want to resume | `make build BUILD_ARGS="--continue-from 04_phase4"` |
| Everything, no picker | `make build BUILD_ARGS=--fresh` |

**Editing a library rebuilds every port of ours**, not only its consumers. A recipe names which
libraries it compiles, and parsing that would be a shell parser inside the package manager — so a
source-less port's recipe hash covers all of `src/libs`. Measured: about two dozen ports in a couple
of minutes. An upstream port's hash is unaffected.

**Never edit a port's sources while its rebuild is running.** The recipe hash is taken at install
time, so an edit mid-build records a hash for a tree that was not what got compiled.

**Do not re-run an early phase on a tree already ahead of it.** Its snapshot would be overwritten
with a later tree filed under the earlier name. That is what `--continue-from` is for.

The mechanics of narrowing — what is suppressed, and why forcing is passed only for what you named
— are in [The build system](build-system.md).

## Iterating without a build at all

Several parts of the system can be exercised on your development host, with no container and no
virtual machine:

```sh
testing/preflight.sh            # the wiring, in seconds
testing/selftest.sh             # the libraries and their consumers, in about half a minute
kdosbuild --preview build 132x43 vt    # a build screen, offscreen
kdos-res --fixture … --dump     # a monitor page, offscreen
kinstall --dry-run              # the installer, executing nothing
```

See [Testing](testing.md) for what each proves.

## When a build fails

In this order:

1. **The failing step's log**, under `build/logs/<phase>/`. The orchestrator names it.
2. **[Build troubleshooting](build-troubleshooting.md)**, which catalogues the recurring failures
   by symptom. Most build failures on this tree are one of those.
3. **The port's own configuration log** inside the work directory, if the failure is a
   configuration script. "C compiler cannot create executables" almost never means what it says —
   the real error is on the failing test program.
4. `testing/preflight.sh`, which catches the dull wiring failures in seconds rather than at the end
   of a two-hour build.

## Rules that apply while building

- **Do not auto-commit.** Commits are the maintainer's.
- **Do not edit a port's sources while its rebuild runs.**
- **Do not run an early phase on a later tree.**
- **A build started inside a backgrounded shell dies with it.** Start long builds so they survive.
- **Clear the cached host helpers when switching between a container run and a host run.** The
  compiled helpers under `ports/` are built against whichever C library ran last, and a binary from
  one cannot execute under the other. The failure does not say so: the version checker simply
  reports that it could not reproduce anything. A container run as root also leaves those
  directories root-owned, so remove them **from a container** rather than reaching for elevated
  privileges.

## The phases

Eight, run in this order. `05_desktop` sorts before `05_phase5` deliberately: the desktop is
ordinary userland and the kernel is the last thing built before packaging.

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

What each contains, and how a phase actually runs, is in [The build system](build-system.md).

## See also

- [The build system](build-system.md) — phases, snapshots, plans, the chroot
- [Writing ports](writing-ports.md) — adding or changing a recipe
- [Build troubleshooting](build-troubleshooting.md) — when it fails
- [Testing](testing.md) — what to run before believing a change
- [Getting started](../02-user-guide/getting-started.md) — the same build, for a user
