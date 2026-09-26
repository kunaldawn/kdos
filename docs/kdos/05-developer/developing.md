# Developing

This page is the starting point for anyone who wants to build KDOS from source or change it. It
covers what your machine needs, how to get from a fresh clone to a bootable ISO, how to rebuild one
small thing instead of everything, how to boot the result, and how a release is cut.

You do not need to have read the rest of the book. When you want more depth:
[The build system](build-system.md) explains the machinery behind `make build`,
[Writing ports](writing-ports.md) covers recipes, and [Testing](testing.md) covers the test
harnesses. Terms such as *port*, *pack* and *box* are defined in the
[glossary](../06-reference/glossary.md).

In short, the path is:

1. Clone the repository and enable the pre-push check.
2. `make fetch` to download every upstream source (the only step that uses the network).
3. `make build` to compile everything into `build/iso-build/kdos.iso`.
4. `make run` to boot it in a virtual machine.
5. After a change, rebuild only what changed, using the table in
   [Rebuilding one thing](#rebuilding-one-thing).

## What a development machine needs

| Need | For | Notes |
|---|---|---|
| Docker | `make build`, `make run-hw` | The build image carries every compiler the build uses |
| Docker or podman | Generating a vendor bundle during `make fetch` | Only when the archive and cache lack one; see below |
| `curl`, `sha256sum` | `make fetch` | All ordinary downloading happens on your machine |
| `git`, `curl`, and network access to the source archive | The pre-push hook | The hook reads hashes with `git grep` and checks each one against the archive with `curl` |
| A C compiler (`$CC`, default `cc`) | `make fetch-check`, `make snapshots`, `ports/fetch --tree`, `ports/publish` | Optional for `make fetch`: without one, the whole fetch runs inside the `kdos-fetch` container |
| `fuser` (from psmisc) | `check-iso-free`, which `make build` runs first | Without it the check is skipped, and nothing stops a build rewriting an ISO a virtual machine is booted from |
| QEMU, OVMF firmware at `/usr/share/ovmf/OVMF.fd`, and `/dev/kvm` | `make run`, `make rundisk` | Only to boot the result |
| Disk space | Everything | See below |

You do not need any language toolchains (Rust, Go, Python, Haskell). The one step that needs them,
generating a vendor bundle, runs them inside a container.

You do not log in as root or use `sudo` for any step, but two things amount to root access. Being
allowed to use Docker is equivalent to root on that machine, since a container can mount any part
of it. And `build/fs`, the target root filesystem, is owned by root, so reading or deleting it takes
root or a container; see [Where the build puts things](#where-the-build-puts-things).

**Disk.** The upstream sources take about 8.9 GB (8.3 GiB): each distinct file is held once in
`ports/.srccache/` and hard-linked into every port directory that names it. Budget tens of gigabytes more for
`build/`, and about 84 GB for a complete set of phase snapshots, of which packaging alone is about
59 GB because it carries the ISO tree. Snapshots are optional; see
[The startup picker](build-system.md#the-startup-picker).

## Getting the source

```sh
git clone <this repository> kdos && cd kdos
git config core.hooksPath script/hooks    # the pre-push check; once per clone
make fetch                                # the only networked step
```

A clone carries recipes, patches and KDOS's own code. The upstream source archives the recipes name
are not in git. `make fetch` downloads each one, checks it against the `sha256 =` line in its recipe,
and puts it in its port directory. After that, `make build` runs with no network at all for as long
as the recipes do not change. Run `make fetch` again after pulling or switching branches; it
downloads only what is new. `make fetch-check` confirms, offline, that nothing is missing or
corrupt.

`git config core.hooksPath script/hooks` points git at `script/hooks/`. Its `pre-push` hook refuses a
push whose recipes name a source the archive does not hold, so a recipe you publish can be fetched
by everyone else; see [Publishing sources](writing-ports.md#publishing-sources). The setting replaces
`.git/hooks` entirely, including any hooks git-lfs installed. The hook also refuses the push when it
cannot reach the archive at all, since it then cannot prove the sources are there; set
`KDOS_SKIP_PUBLISH_CHECK=1` for a push you know is safe.

### Where sources come from

Every source file is stored in the repository `kunaldawn/kdos` as a release asset named by
its own SHA-256 hash, in one of 256 releases keyed by the hash's first two hex digits:

```
https://github.com/kunaldawn/kdos/releases/download/sha256-<first two hex digits>/<sha256>
```

For example, a file whose recipe hash starts `bb32…` lives at `…/download/sha256-bb/bb32…`. The
recipe hash, the asset's name and the digest GitHub computes for the asset are the same string, so no
index is needed, and a file that verifies is the file the recipe meant whichever route it came by.
The archive is append-only: an asset whose digest matches its name is never replaced or deleted, so
an old checkout can still find the exact bytes it was written against after upstream has moved on
or gone. There are 256 releases because a GitHub release holds at most 1000 assets.
`ports/srclib.sh` holds this addressing, and `ports/fetch`, `ports/publish` and the pre-push hook all
read it from there.

`ports/fetch` resolves each file a `sha256 =` line names, other than files git tracks itself (such as
patches), by trying these locations in order and stopping at the first copy that verifies:

| | Location |
|---|---|
| 1 | The port directory: a file already there |
| 2 | The cache, `ports/.srccache/sha256-XX/<hash>` |
| 3 | The archive, `$KDOS_SOURCES_BASE/sha256-XX/<hash>` |
| 4 | The recipe's `source =` URL for that file |
| 5 | Generation, only for the port's own `<name>-vendor-<version>.tar.xz` |

A copy that fails its hash, or is empty, is removed and the next location is tried. If the archive
cannot be reached, the fetch falls through to upstream.

**The cache.** Every copy that verifies is entered into the cache, and the port directory holds the
same file. Both are hard links when the cache and the checkout share a filesystem, so the cache
costs no extra space; otherwise each is a copy, so a shared cache on another disk holds a second
copy of every source. Either way, switching branches downloads nothing already fetched. The cache is laid out exactly like the archive, is
ignored by git, and survives `make clean`; deleting it costs a full re-download and nothing else. It
belongs to one checkout. A second checkout or a fresh clone starts with an empty cache unless
`KDOS_SRCCACHE` points both at a shared path, or unless it is fetched with `ports/fetch --tree <dir>`
from a checkout that already has the cache.

**Unhashed files.** A file named by a recipe with no hash yet (you have just bumped `version =`) is
downloaded from upstream only, with a warning, and never from the archive.

**Leftover archives.** Source files are not tracked by git, so a pull or branch switch that moves a
port to another version leaves the old file in the port directory. Nothing the build reads names it;
`testing/preflight.sh` lists it as a stale fetched archive, and it is safe to delete.

**Interrupted downloads.** A download goes to `<file>.part` and is renamed into place only once it
verifies. The next run resumes the partial file. If a resumed download fails its hash, it is fetched
once more from the beginning from the same URL before the next location is tried, because the partial
file may have held the start of different bytes.

**Vendor bundles.** Rust, Go, Python and Haskell ports build offline from a vendor bundle: a tarball
of their dependencies, generated by the language's own package manager at the version this tree
compiles with. A default `make fetch` therefore runs in two passes. Your machine resolves everything
it can from the first four locations with `curl` and `sha256sum`. Only a port whose vendor bundle is
still missing is handed to the `kdos-fetch` container, which generates it; a clone whose archive is
complete never builds that image. A port missing any other file is not handed over, because its
bundle cannot be generated without its source, and fails on your machine. A generated bundle is held
to its recipe hash like a download: bundles are built reproducibly (sorted names, a fixed timestamp,
owner 0, `xz -9 -T1`), so the same inputs give the same bytes.

| Command | Does |
|---|---|
| `ports/fetch [port…]` | Fetch every port, or only the named ones |
| `ports/fetch --check [port…]` | Offline: list each archived file that is missing or fails its hash, and each recipe that cannot be read; exit 1 if any. `make fetch-check` runs it over every port |
| `ports/fetch --tree <dir> [port…]` | Fetch for the `ports/core` of another checkout `<dir>`, using this tree's archive logic, recipe reader and cache. Never generates a vendor bundle |

A checkout with no `ports/srclib.sh` (any tag or branch whose `ports/fetch` cannot read the archive)
cannot fetch from the archive by itself, and this tree's `ports/fetch` cannot simply be copied into
it because it needs `srclib.sh` beside it. Fetch for it from a checkout that has one instead:

```sh
ports/fetch --tree ../kdos-old
```

In that mode, a file the other checkout tracks through a Git LFS filter counts as an archive to
fetch, because without LFS its working copy is only a pointer.

| Variable | Default | Effect |
|---|---|---|
| `KDOS_SOURCES_REPO` | `kunaldawn/kdos` | The archive repository |
| `KDOS_SOURCES_BASE` | `https://github.com/$KDOS_SOURCES_REPO/releases/download` | The archive's download base. Set it empty to use upstream only |
| `KDOS_SRCCACHE` | `ports/.srccache` | The cache. A path outside the repository is mounted into the fetch container |
| `KDOS_FETCH_HOST` | unset | `1`: one pass on your machine, generating vendor bundles with its own toolchains |
| `ENGINE_OUT` | `docker` if installed, else `podman` | The container engine for generating vendor bundles |
| `CC` | `cc` | The compiler for the recipe reader, `ports/.kpkgbin/kpkg` |

## The first build

```sh
make build            # compile everything; no network, after make fetch
```

`make build` builds the `os-dev` container image from the repository's `Dockerfile`, then runs the
orchestrator inside it with `--network none`, `--privileged` and eight CPUs. The repository is
mounted with `build/` writable and `src/`, `fs/`, `script/` and `ports/` read-only. Your user and
group ids are passed in, so the results are handed back to you rather than left owned by root.

On a terminal, the build first opens a picker that asks whether to start fresh or restore a snapshot,
and whether to write snapshots as it goes; see
[The startup picker](build-system.md#the-startup-picker). A full build from scratch takes many hours.
The finished ISO is `build/iso-build/kdos.iso`.

Two things to know before the first run:

- **The ISO carries no applications.** The medium ships the application catalogue, and an
  application is built by podman on the machine that wants it, from the store or with
  `kdos app install`. An exported set of packs imports offline.
- **The build refuses to overwrite an ISO a virtual machine is using.** QEMU reads the image lazily,
  so rewriting it under a running guest turns every block the guest has not cached into an I/O
  error. Shut the guest down first, or override with `make build ALLOW_ISO_IN_USE=1`.

Two opt-in variables change what the ISO carries:

| Variable | Effect |
|---|---|
| `KDOS_ISO_SOURCES=1` | Copies `ports/` (with every fetched source), `src/` and `script/` onto the ISO under `/sources`, with a `SOURCES` stamp naming the tree they came from. `fs/`, the `Makefile` and the `Dockerfile` are not mounted into the build under `make build`, so they are not on the medium. Roughly doubles the image |
| `KDOS_PACK_KDOS=1` | Also packs the root filesystem as the base pack `kdos`, in `build/kdos-base` |

For example, `make build KDOS_ISO_SOURCES=1`.

## Make targets

| Target | Does | Needs |
|---|---|---|
| `all` | The default target; the same as `build` | |
| `build` | The whole build, in the container. Runs `check-iso-free` first | Docker |
| `fetch` | Fetch every port's sources into `ports/core`, generating vendor bundles the archive lacks. `ports/fetch <port>` narrows it | Network; a container only to generate |
| `fetch-check` | List, offline, every archived source that is missing or fails its hash | A C compiler |
| `updates` | Check every port for a newer upstream release | Network, `curl`, `git` |
| `snapshots` | List the phase snapshots | A C compiler |
| `run` | Boot the ISO in a virtual machine, with `build/kdos.qcow2` attached as a disk (created at 20 GB if missing) | QEMU, OVMF |
| `rundisk` | Boot `build/kdos.qcow2` instead of the ISO | QEMU, OVMF, an existing disk image |
| `run-hw` | Boot the ISO with hardware-accelerated graphics | Docker, the NVIDIA Container Toolkit |
| `rundisk-hw` | The same, from the disk image | Docker, the NVIDIA Container Toolkit |
| `debug-boot` | Boot the built kernel and initramfs directly, with the serial console on your terminal, for early-boot debugging | QEMU |
| `check-iso-free` | Refuse to rewrite an ISO a process has open | `fuser`; without it the check is skipped |
| `check-hw` | Check the accelerated-graphics setup: an error without Docker, warnings without the `nvidia` runtime or `/dev/udmabuf` | |
| `cleandisk` | Replace `build/kdos.qcow2` with a new, empty 20 GB disk image | QEMU |
| `cleanbuild` | Delete everything in `build/` except `snapshots` and `keys`, and except what only root can remove (see below) | |
| `clean` | Delete everything in `build/` except `keys`, and except what only root can remove | |

Both cleans run on your machine as your user. `build/fs` is owned by root, so they cannot remove it,
and the target ignores the failure: after `make clean` the old root filesystem is still there, and
the next build runs on it. Remove it from a container, or as root:

```sh
docker run --rm -v "$PWD/build:/b" os-dev rm -rf /b/fs
```

The `os-dev` image exists once `make build` has run.

Arguments reach the orchestrator through `BUILD_ARGS` and the version checker through `PORTUP_ARGS`:

```sh
make build BUILD_ARGS="--continue-from 04_phase4"
make updates PORTUP_ARGS="--check curl"
```

`ports/update --check` exits 1 when it finds an update, so `make updates` treats status 1 as success
and fails only on status 2 or higher.

## Running the result

```sh
make run          # plain graphics: the desktop works, the phosphor pass does not
make run-hw       # accelerated: the phosphor pass is on
```

The *phosphor pass* is the compositor's CRT-style post-processing shader. `make run` uses a plain
virtual display, on which the compositor falls back to software rendering, and the phosphor pass
turns itself off there because a full-screen shader in software is far too slow. `make run-hw` runs
a containerised QEMU 10 with GPU-accelerated graphics (virgl) through
`testing/qemu-hw/run.sh`, which is the configuration where the shader is actually visible.

Both come up at 1920x1080. `KDOS_RES` changes that for every run target:

```sh
make run KDOS_RES=2560x1440
```

KDOS never asks for a display mode; the desktop takes the one the virtual screen says it prefers.
For QEMU's virtio GPU that is whatever its `xres` and `yres` properties say, and left unset they
default to 1280x800, which is too small to lay out the Start menu's three columns. `KDOS_RES` sets
those two properties, the `Makefile` passes it to `testing/qemu-hw/run.sh`, and
`testing/vnc-shot.py` sets the same properties from its own `--size`.

The accelerated run scales the picture to fit its window instead of resizing the guest
(`zoom-to-fit=on`). Otherwise QEMU's window would report its own size to the guest, and the desktop
would follow whatever size the window happened to open at. `KDOS_QEMU_DISPLAY` overrides the display
option: `KDOS_QEMU_DISPLAY=gtk,gl=es` makes the guest follow the window, and
`KDOS_QEMU_DISPLAY=egl-headless` runs with no window at all.

## Where the build puts things

| Path | Holds | Notes |
|---|---|---|
| `build/fs` | The target root filesystem | Owned by root on purpose; see below |
| `build/iso-build/kdos.iso` | The ISO | |
| `build/logs/<phase>/` | One log per step | The first thing to read when a build fails |
| `build/logs/chroot.log` | Mount and unmount messages from entering the chroot | |
| `build/snapshots/<phase>/` | Phase snapshots | Kept by `cleanbuild` |
| `build/keys/` | Signing keys | Kept by both cleans; never committed |
| `build/kdos.qcow2` | The virtual machine's disk | Created by `make run` |
| `build/.kdosbuild` | The compiled orchestrator | Rebuilt at the start of every build |
| `build/.devplan.json` | The build plan in force | |
| `build/kdos-base/` | The base pack, with `KDOS_PACK_KDOS=1` | |
| `build/fetch-home` | The fetch container's home and toolchain caches | Written as your user |
| `build/freeze/` | `sources-<tag>.sha256`, written by `ports/publish --freeze` | |

`build/` is ignored by git in its entirety.

Some tools keep compiled helpers and data under `ports/`, all ignored by git:

| Path | Holds |
|---|---|
| `ports/.srccache/` | The source cache. Plain data; survives both cleans |
| `ports/.kpkgbin/` | The recipe reader `ports/fetch` and `ports/publish` compile |
| `ports/.portup` | The version checker `ports/update` compiles |
| `ports/.portup-tools/` | The version checker's own copy of the recipe reader |
| `ports/.update-cache.json` | The version checker's results, kept for 24 hours |

`build/fs` is deliberately not handed back to your user. Changing ownership across that tree would
clear every setuid bit (the password checker, the resource helper, and the two user-namespace mapping
helpers without which no container can start), and would leave the account and privilege files owned
by an ordinary user. Reading it from your machine needs a container or root, which is the right price
for a root filesystem.

## Rebuilding one thing

A full build takes hours, and almost no change needs one. Find what you changed in this table:

| Changed | Run |
|---|---|
| Something under `fs/` | `make build BUILD_ARGS="--phases 01_phase1,06_packaging --steps 01_phase1:00_file_system.sh"` |
| One port's recipe | `make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild <port>"` |
| A desktop program | `make build BUILD_ARGS="--phases 05_desktop --rebuild <port>"` |
| A library under `src/libs/` | `make build BUILD_ARGS="--phases 01_phase1,04_phase4,05_desktop,06_packaging --steps 01_phase1:12_kpkg.sh,01_phase1:13_kinstall.sh"`; see below |
| Only packaging | `make build BUILD_ARGS="--phases 06_packaging"` |
| Nothing; resuming an interrupted run | `make build BUILD_ARGS="--continue-from 04_phase4"` |
| Every phase, on the existing tree, skipping the startup picker | `make build BUILD_ARGS=--fresh` |

Use the phase the port is listed in: a port named in `script/03_phase3/packages.txt` is rebuilt with
`--phases 03_phase3,06_packaging`. Add `06_packaging` whenever you want a new ISO from the change.
To find the phase:

```sh
grep -l '^<port>$' script/*/packages.txt     # the phase that names it
ls build/logs/*/ | grep <port>              # the phase whose logs hold its install
```

A quarter of the ports are named in no `packages.txt` (250 of the 1,014 in `ports/core`); almost
all of them are installed because a listed port depends on them. For those, the second command finds the phase, or
use the phase of the first listed port that depends on it.

**Editing a library rebuilds every port of KDOS's own**, not only the ones that use it. A recipe for
one of KDOS's programs has no upstream tarball, so its recipe hash covers the whole of `src/libs`;
working out which libraries a `build.sh` actually compiles would need a shell parser inside the
package manager. That is 24 recipes, and takes a couple of minutes. Upstream ports are unaffected.
Those recipes are installed in `04_phase4` and `05_desktop`, and their changed hash is enough to
rebuild them, so they need no `--rebuild`.

Three programs link the libraries outside any recipe. The orchestrator is recompiled at the start of
every build. The package manager `kpkg` and the installer `kinstall` are compiled by the phase-1
steps `12_kpkg.sh` and `13_kinstall.sh`, and rebuilt only when those steps run, which is why the
command above names them. Leave the `01_phase1` part out when your change touches neither.

What a plan suppresses, and why only the named ports are forced, is explained in
[Build plans](build-system.md#build-plans).

For a desktop program there is a faster loop still: `testing/quick.sh` builds just that program and
patches it into a booted ISO, without repacking anything. See
[Testing](testing.md#the-fast-loop).

### Building from scratch

`--fresh`, and *start fresh* in the picker, run every phase on the tree already in `build/`. They
do not empty it: each toolchain and phase-1 script exits at once when its marker under `build/mark/`
exists, and `kpkg` skips every port already installed from the same recipe. To build from nothing,
empty the three trees that record that progress first. `build/fs` belongs to root, so do it from a
container:

```sh
docker run --rm -v "$PWD/build:/b" os-dev rm -rf /b/fs /b/mark /b/cross
make build BUILD_ARGS=--fresh
```

Budget most of a day for the build that follows, and about 84 GB more if it writes snapshots.

## Things to avoid while a build runs

- **Do not edit a port's sources while it is being rebuilt.** The recipe hash is taken when the port
  is installed, so an edit in the middle records a hash for a tree that is not what got compiled.
- **Do not re-run an early phase on a tree that is already past it.** Its snapshot would be
  overwritten with the later tree, filed under the earlier phase's name. `--continue-from` exists for
  exactly this case.
- **Run long builds where they will survive your shell.** A build started from a shell that exits
  dies with it; use `tmux`, `screen` or similar.
- **Clear the compiled helpers after running a `ports/` tool in a container.** `make build` mounts
  `ports/` read-only and `make fetch`'s container keeps its own recipe reader under
  `build/fetch-home`, so neither touches them. Running `ports/update`, `ports/fetch` or
  `ports/publish` yourself inside a container that mounts the repository read-write (the rig image
  or a musl development image, for example) does. `ports/.portup`, `ports/.portup-tools` and
  `ports/.kpkgbin` are compiled against whichever C library ran last (musl in an Alpine container,
  usually glibc on your machine), and a binary built against one cannot run under the other. The
  failure does not say so: the version checker just reports that it reproduced nothing. A container
  that runs as root also leaves those paths owned by root, so remove them from a container rather
  than with `sudo`. The source cache, `ports/.srccache`, is plain data and does not need clearing.

## Working without a build at all

Much of the system can be checked on your development machine, with no container and no virtual
machine:

```sh
testing/preflight.sh                              # the wiring: about two minutes
testing/selftest.sh                               # libraries and their consumers: about half a minute
make snapshots                                    # compiles build/.kdosbuild for this machine
build/.kdosbuild --preview build 132x43 vt        # a build screen, drawn as text
```

`testing/selftest.sh` compiles `kdosbuild`, `kdos-res`, `kinstall` and the other programs it tests
into a temporary directory, which it deletes when it exits. Among other things it draws monitor pages
as text (`kdos-res --fixture <dir> --dump`) and runs the installer executing nothing
(`kinstall --dry-run`). Neither program is installed on a development machine, so those two forms
are what the self-test runs, not commands to type.

`build/.kdosbuild` runs on your machine only when your machine's compiler built it, which
`make snapshots` does. `make build` replaces it with a musl binary compiled in the build image, which
a glibc machine cannot run; the shell then reports `No such file or directory` without mentioning the
missing musl loader. Run `make snapshots` again after a build, or keep a separate copy with
`KDOSBUILD_BIN=build/.kdosbuild-host script/kdosbuild.sh --list`.

The window model needs no display at all. `libkwm` links only `libkbase`, so the self-test replays
`testing/fixtures/wm/geometry.txt` on any machine in milliseconds. It is the fastest way to answer a
question about where a window lands, what a tiled window becomes, or which edge a moving edge stops
against.

## When a build fails

Work through these in order:

1. **The failing step's log**, under `build/logs/<phase>/`. The orchestrator names it, and in the
   failure panel `O` opens it and `C` copies its path.
2. **[Build troubleshooting](build-troubleshooting.md)**, which lists the recurring failures by
   symptom. Most build failures on this tree are one of those.
3. **The port's own configure log** inside its work directory, if a configure script failed.
   `C compiler cannot create executables` rarely means what it says; the real error is in the log,
   next to the test program that failed.
4. **`testing/preflight.sh`**, which catches dull wiring mistakes (a missing dependency, an unlisted
   port) before a build that runs for hours finds them.

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

`05_desktop` sorts before `05_phase5` on purpose: the desktop is ordinary userland, and the kernel is
the last thing built before packaging. What each phase contains, and how one runs, is in
[The build system](build-system.md#phases).

## Cutting a release

A KDOS release is three things: a git tag, an ISO, and a list of every source hash the tag's
recipes name. The list is one file that says what the release needs to build; the hashes
themselves are pinned by the tag, whose recipes git cannot change without changing the tag.

```sh
git tag v0.3 && git push origin v0.3
make fetch && make build                  # from the tag
# create a DRAFT release v0.3 on kunaldawn/kdos, attach build/iso-build/kdos.iso
ports/publish --freeze v0.3
# publish the draft
```

`ports/publish --freeze <tag>`:

1. Extracts the tag's recipes with `git archive`, so the list reflects the tag and not your working
   tree.
2. Writes `build/freeze/sources-<tag>.sha256` in `sha256sum` format, one `<hash>  <port>/<file>`
   line per source, sorted.
3. Checks every hash against the archive, and exits 1 listing any that are missing. Publish those
   with `ports/publish` first.
4. Attaches the list as `sources.sha256` to the release `<tag>` on `$KDOS_REPO` (default
   `kunaldawn/kdos`), creating a draft release pointing at the tag when none exists.

It never replaces an existing `sources.sha256` with different contents; it exits 2 instead. With
`--dry-run` or `--check` it stops after step 3 and says what it would attach. Uploading needs a token
in `$KDOS_SOURCES_TOKEN` or `~/.config/kdos/sources-token` (readable only by you); see
[Publishing sources](writing-ports.md#publishing-sources).

The release starts as a draft so the ISO and the list go out together when you publish it. Leave
GitHub's immutable releases **off** on `kunaldawn/kdos`: the source archive's 256 `sha256-XX`
releases live in the same repository, the setting applies to all of them, and it would freeze each
shard at its first publication, after which no new source could be added to it.

## See also

- [The build system](build-system.md) — phases, snapshots, build plans, the chroot
- [Writing ports](writing-ports.md) — adding or changing a recipe, and publishing its sources
- [Build troubleshooting](build-troubleshooting.md) — recurring failures, by symptom
- [Testing](testing.md) — what to run before believing a change
- [The C libraries](c-libraries.md) — what KDOS programs are built on
- [Getting started](../02-user-guide/getting-started.md) — the same build, from a user's side
