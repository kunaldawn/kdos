# Developing

This chapter is the working manual for anyone who builds KDOS from source or changes it. It covers
what a development machine needs, how to get from a fresh clone to a bootable ISO, how to rebuild
one small thing instead of everything, how to boot and inspect the result, what can be checked with
no build at all, and how a release is cut. It assumes you know what KDOS is; for the build told as
one continuous story from `git clone` to a bootable image, read
[How KDOS is built](how-kdos-is-built.md) first. The mechanism behind `make build` is in
[The build system](build-system.md), the recipe format in [Writing ports](writing-ports.md), and
the test harnesses in [Testing](testing.md). Terms such as *port*, *pack* and *box* are defined in
the [glossary](../06-reference/glossary.md).

The path from a clone to a running system has five steps:

1. Clone the repository and enable the pre-push check.
2. Run `make fetch` to download every upstream source. It is the only step the build needs the
   network for; apart from building container images, `make build` downloads nothing.
3. Run `make build` to compile everything into `build/iso-build/kdos.iso`.
4. Run `make run` to boot the ISO in a virtual machine.
5. After a change, rebuild only what changed, using the table in
   [Rebuilding one thing](#rebuilding-one-thing).

## What a development machine needs

| Need | For | Notes |
|---|---|---|
| Docker | `make build`, `make run-hw` | The build image carries every compiler the build uses |
| Network access for image builds | The first `make build`, the first `make run-hw` | Building the `os-dev` image pulls Alpine and its packages, again whenever the `Dockerfile` changes; the `run-hw` image is built from Ubuntu on first use |
| Docker or podman | Generating a vendor bundle during `make fetch` | Only when the archive and the cache lack one; see [Where sources come from](#where-sources-come-from) |
| `curl`, `sha256sum` | `make fetch` | All ordinary downloading happens on your machine |
| `git`, `curl`, and network access to the source archive | The pre-push hook | The hook reads hashes with `git grep` and checks each one against the archive with `curl` |
| A C compiler (`$CC`, default `cc`) | `make fetch-check`, `make snapshots`, `ports/fetch --tree`, `ports/publish`, `testing/preflight.sh`, `testing/selftest.sh`, `make updates` | Optional for `make fetch`: without one, the whole fetch runs inside the `kdos-fetch` container. `make updates` (`ports/update`) always runs `cc` and ignores `$CC` |
| `fuser` (from psmisc) | `check-iso-free`, which `make build` runs first | Without it the check is skipped, and nothing stops a build rewriting an ISO a virtual machine is booted from |
| QEMU, OVMF firmware at `/usr/share/ovmf/OVMF.fd`, and `/dev/kvm` | `make run`, `make rundisk` | Only to boot the result. Both pass `-enable-kvm -cpu host`, so without `/dev/kvm` they print a warning and QEMU then refuses to start |
| Disk space | Everything | See below |

No language toolchain (Rust, Go, Node, Python, Haskell) is needed on your machine. The one step
that uses them, generating a vendor bundle, runs them inside a container.

No step needs you to log in as root or use `sudo`, but two things amount to root access. Permission
to use Docker is equivalent to root on that machine, since a container can mount any part of it.
And `build/fs`, the target root filesystem, is owned by root, so reading or deleting it takes root
or a container; see [Where the build puts things](#where-the-build-puts-things).

**Disk.** The upstream sources for the current tree take about 41.5 GB (38.6 GiB): the 2,450
distinct files the recipes fetch, which is what `ports/.srccache/` holds after a complete
`make fetch`. Each distinct file is held once in the cache and hard-linked into every port directory that names it. Budget tens of gigabytes more for
`build/`, and about 84 GB for a complete set of phase snapshots (measured on one full build), of
which the packaging snapshot alone is about 59 GB because it carries the ISO tree. Snapshots are
optional; see [The startup picker](build-system.md#the-startup-picker).

## Getting the source

```sh
git clone <this repository> kdos && cd kdos
git config core.hooksPath script/hooks    # the pre-push check; once per clone
make fetch                                # downloads every source
```

A clone carries recipes, patches and KDOS's own code. The upstream source archives the recipes
name are not in git. `make fetch` downloads each one, checks it against the `sha256 =` line in its
recipe, and puts it in its port directory. After that, the build itself runs with no network (the
orchestrator's container is started with `--network none`) for as long as the recipes do not change.
Only building the `os-dev` image, the first time or after the `Dockerfile` changes, downloads
Alpine packages. Run `make fetch` again after pulling or switching branches; it
downloads only what is new. `make fetch-check` confirms, offline, that nothing is missing or
corrupt.

`git config core.hooksPath script/hooks` points git at `script/hooks/`. Its `pre-push` hook
refuses a push whose recipes name a source the archive does not hold, so that a recipe you publish
can be fetched by everyone else. Where each source is kept, how `make fetch` finds it and what the
hook checks are described in [Where sources come from](#where-sources-come-from).

## The first build

```sh
make build            # compile everything; the orchestrator runs with no network
```

`make build` builds the `os-dev` container image from the repository's `Dockerfile` (Alpine 3.23
with GCC, musl, bash and the other tools the orchestrator needs), then runs the orchestrator inside
it with `--network none`, `--privileged` and eight CPUs. The repository is mounted with `build/`
writable and `src/`, `fs/`, `script/` and `ports/` read-only. The current commit and whether the
working tree is dirty are passed in and recorded with each snapshot. Your user and group ids are
passed in as well, and when the orchestrator exits it hands everything under `build/` back to you,
except `build/fs`; see [Where the build puts things](#where-the-build-puts-things).

The orchestrator is `kdosbuild`, compiled from `src/build/kdosbuild/` by `script/kdosbuild.sh` at
the start of every build. On a terminal it first opens a picker that asks whether to start fresh or
restore a snapshot, and whether to write snapshots as it goes; see
[The startup picker](build-system.md#the-startup-picker). Without a terminal it prints plain lines
instead, so a build can be logged to a file. A full build from scratch takes many hours. The
finished ISO is `build/iso-build/kdos.iso`.

Two things to know before the first run:

- **The ISO carries no applications.** The medium ships the application
  [catalogue](../06-reference/glossary.md), and an application is built by podman on the machine
  that wants it, from the *Applications* window in the Start menu
  ([Applications](../02-user-guide/applications.md#the-store)) or with `kdos app install`. An
  exported set of packs imports offline.
- **The build refuses to overwrite an ISO a virtual machine is using.** QEMU reads the image lazily,
  so rewriting it under a running guest turns every block the guest has not cached into an I/O
  error. Shut the guest down first, or override with `make build ALLOW_ISO_IN_USE=1`.

Two opt-in variables change what the ISO carries. Each is passed by the `Makefile` and forwarded
into the chroot by `script/chroot_exec.sh`:

| Variable | Effect |
|---|---|
| `KDOS_ISO_SOURCES=1` | Copies `src/` and `script/` onto the ISO under `/sources`, beside `system.sfs` rather than inside it, with a `SOURCES` stamp giving the port count, the size and the build time. `sources/ports/` is on the medium but empty: the step copies the chroot's `/kdos/ports`, and `/kdos` is a non-recursive bind of the repository in which `ports/` is only a mount point (the recipes and fetched sources are mounted at `/ports` instead). `fs/` is not copied, and the `Makefile` and `Dockerfile` are not mounted into the build, so none of them is on the medium |
| `KDOS_PACK_KDOS=1` | Also packs the root filesystem as the base pack `kdos`, in `build/kdos-base/kdos.kpack`, and puts it on the ISO under `/packs`. `kdos-box create ports base=pack:kdos` then gives a running KDOS a clean KDOS to build ports in. Without the flag, a pack left from an earlier build is deleted |

For example, `make build KDOS_ISO_SOURCES=1`.

## The phases

The orchestrator runs eight phases, each a directory under `script/`, in sorted order:

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

`05_desktop` sorts before `05_phase5` by design: the desktop is ordinary userland, and the kernel is
the last thing built before packaging. `00_toolchain`, `01_phase1` and `06_packaging` are
directories of numbered scripts; the other five install the ports named in their `packages.txt`.
What each phase contains, and how one runs, is in [The build system](build-system.md#phases).

## Make targets

| Target | Does | Needs |
|---|---|---|
| `all` | The default target; the same as `build` | |
| `build` | The whole build, in the container. Runs `check-iso-free` first | Docker |
| `fetch` | Fetch every port's sources into `ports/core`, generating vendor bundles the archive lacks. `ports/fetch <port>` narrows it | Network; a container only to generate |
| `fetch-check` | List, offline, every archived source that is missing or fails its hash | A C compiler |
| `updates` | Check every port for a newer upstream release | Network, `curl`, `git`, `cc` |
| `snapshots` | List the phase snapshots, compiling `build/.kdosbuild` with your machine's compiler | A C compiler |
| `run` | Boot the ISO in a virtual machine, with `build/kdos.qcow2` attached as a disk (created at 20 GB if missing) | QEMU, OVMF |
| `rundisk` | Boot `build/kdos.qcow2` instead of the ISO | QEMU, OVMF, an existing disk image |
| `run-hw` | Boot the ISO with hardware-accelerated graphics | Docker, the NVIDIA Container Toolkit |
| `rundisk-hw` | The same, from the disk image | Docker, the NVIDIA Container Toolkit |
| `debug-boot` | Boot the built kernel and initramfs directly, with the serial console on your terminal, for early-boot debugging | QEMU, a finished build |
| `check-iso-free` | Refuse to rewrite an ISO a process has open | `fuser`; without it the check is skipped |
| `check-hw` | Check the accelerated-graphics setup: an error without Docker, warnings without the `nvidia` runtime or `/dev/udmabuf` | |
| `cleandisk` | Replace `build/kdos.qcow2` with a new, empty 20 GB disk image | QEMU |
| `cleanbuild` | Delete everything in `build/` except `snapshots` and `keys`, and except what only root can remove (see below) | |
| `clean` | Delete everything in `build/` except `keys`, and except what only root can remove | |

Both cleans run on your machine as your user. `build/fs` is owned by root, so they cannot remove
it, and the target ignores the failure: after `make clean` the old root filesystem is still there,
and the next build runs on it. Remove it from a container, or as root:

```sh
docker run --rm -v "$PWD/build:/b" os-dev rm -rf /b/fs
```

The `os-dev` image exists once `make build` has run.

Arguments reach the orchestrator through `BUILD_ARGS` and the version checker through
`PORTUP_ARGS`:

```sh
make build BUILD_ARGS="--continue-from 04_phase4"
make updates PORTUP_ARGS="--check curl"
```

The orchestrator options used most often are these; the full list is in
[The build system](build-system.md#flags):

| Option | Effect |
|---|---|
| `--fresh` | Skip the startup picker and run every phase on the existing tree |
| `--restore PHASE` | Restore a snapshot and continue after it (`PHASE` is a name, a directory, a 1-based index or `latest`) |
| `--continue-from PHASE` | Resume at `PHASE` on the existing tree, restoring nothing |
| `--phases LIST` | Run only these phases |
| `--steps LIST` | Run only these scripts, each written `PHASE:script.sh` |
| `--rebuild LIST` | Rebuild these ports even though they are installed |
| `--no-snapshot` | Write no snapshots during this build |
| `--plain`, `--json` | No interface: plain lines, or one JSON object per event |

`ports/update --check` exits 1 when it finds an update, so `make updates` treats status 1 as
success and fails only on status 2 or higher.

## Running the result

```sh
make run          # plain graphics: the desktop works, the phosphor pass does not
make run-hw       # accelerated: the phosphor pass is on
```

The *phosphor pass* is the compositor's CRT-style post-processing shader. `make run` uses a plain
virtual display (`virtio-vga` with QEMU's GTK window), on which the compositor falls back to
software rendering, and the phosphor pass turns itself off there because a full-screen shader in
software is far too slow. `make run-hw` runs a containerised QEMU 10 with GPU-accelerated graphics
(virgl) through `testing/qemu-hw/run.sh`, which is the configuration where the shader is visible.

`make run` and `make rundisk` give the guest 4 GB of memory and as many CPUs as your machine has,
put the serial console on your terminal, and use QEMU's user-mode networking. `make run` also
attaches a USB tablet, so the pointer tracks the window without being grabbed. Every run target
asks `testing/qemu-audio.sh` for a sound card: it probes which audio backend the local QEMU can use
and adds an Intel HDA controller when there is one, or nothing when the host cannot play sound.

All run targets come up at 1920x1080. `KDOS_RES` changes that:

```sh
make run KDOS_RES=2560x1440
```

KDOS never asks for a display mode; the desktop takes the one the virtual screen says it prefers.
For QEMU's virtio GPU that is whatever its `xres` and `yres` properties say, and left unset they
default to 1280x800, which is too small to lay out the Start menu's three columns. `KDOS_RES` sets
those two properties, the `Makefile` passes it to `testing/qemu-hw/run.sh`, and
`testing/vnc-shot.py` sets the same properties from its own `--size` (whose default is 1280x800).

The accelerated run scales the picture to fit its window instead of resizing the guest
(`zoom-to-fit=on`). Otherwise QEMU's window would report its own size to the guest, and the desktop
would follow whatever size the window happened to open at. `KDOS_QEMU_DISPLAY` overrides the display
option: `KDOS_QEMU_DISPLAY=gtk,gl=es` makes the guest follow the window, and
`KDOS_QEMU_DISPLAY=egl-headless` runs with no window at all.

`make debug-boot` boots `build/fs/boot/vmlinuz-kdos` and `build/fs/boot/initramfs.cpio.gz` directly,
with the ISO attached and the kernel's console on both the screen and the serial port (`-serial
stdio`), so the serial console prints in your terminal. The kernel command line carries `quiet
loglevel=3`, the same as the default Limine entry, so only kernel messages at error level or above
appear there. For every kernel message, boot the `KDOS Live (verbose)` entry from the Limine menu
under `make run`, or edit the `-append` line of the `debug-boot` target. The target uses no KVM and
no firmware.

## Where the build puts things

| Path | Holds | Notes |
|---|---|---|
| `build/fs` | The target root filesystem | Owned by root on purpose; see below |
| `build/iso-build/kdos.iso` | The ISO | |
| `build/logs/<phase>/` | One log per step | The first thing to read when a build fails |
| `build/logs/chroot.log` | Mount and unmount messages from entering the chroot | |
| `build/snapshots/<phase>/` | Phase snapshots | Kept by `cleanbuild` |
| `build/mark/` | The early phases' "already done" markers | See [Building from scratch](#building-from-scratch) |
| `build/cross/` | The cross toolchain (binutils and GCC for `x86_64-kdos-linux-musl`) that the first two phases compile with | |
| `build/tmp/` | Scratch space for the early phases | Emptied at the start of every step of those phases |
| `build/keys/` | Kept by both cleans | Nothing in the build writes it; it is set aside so a signing key kept there survives a clean |
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
| `ports/.kpkgbin/` | The recipe reader `ports/fetch` and `ports/publish` compile, recompiled whenever its sources are newer than the binary |
| `ports/.portup` | The version checker `ports/update` compiles, recompiled when a `.c` file under `src/tools/kdos-portup/` is newer than the binary; a change to a library it links does not trigger it |
| `ports/.portup-tools/` | The version checker's own copy of the recipe reader, compiled when missing or when it cannot run; a change to its sources does not trigger it |
| `ports/.update-cache.json` | The version checker's results, kept for 24 hours |

Those helpers are compiled against whichever C library ran them last: musl in an Alpine container,
usually glibc on your machine. A binary built against one cannot run under the other, and the
failure does not say so: `ports/fetch` and `ports/publish` report every recipe as unreadable
(`cannot read its kpkgbuild`, `kpkg cannot read its recipe`), and `ports/update` fails to start its
checker with the shell's `No such file or directory`. The version checker's own recipe reader under
`ports/.portup-tools` is tested before each use and rebuilt when it exits with status 127, so it
recovers from a C-library switch by itself; the other two do not. `make build` mounts
`ports/` read-only and `make fetch`'s container keeps its own recipe reader under
`build/fetch-home`, so neither touches them. Running `ports/update`, `ports/fetch` or
`ports/publish` yourself inside a container that mounts the repository read-write does, for example
any Alpine-based development container. Delete `ports/.portup`, `ports/.portup-tools` and
`ports/.kpkgbin` afterwards and let the next run recompile them. A container that runs as root
leaves those paths owned by root, so remove them from a container rather than with `sudo`. The
source cache, `ports/.srccache`, is plain data and does not need clearing.

`build/fs` is deliberately not handed back to your user. Changing ownership across that tree would
clear every setuid bit (the password checker, the resource helper, and the two user-namespace mapping
helpers without which no container can start), and would leave the account and privilege files
owned by an ordinary user. Reading it from your machine needs a container or root.

## Where sources come from

Every source file that git does not carry is stored in the repository `kunaldawn/kdos` as a release
asset named by its own SHA-256 hash. The assets fill numbered releases in order: `sources-001`
holds the first 1,000 files (GitHub's limit on assets per release), `sources-002` the next 1,000,
and so on. The committed file `ports/sources.idx` says which release holds which hash, one line
per file, sorted by hash. At the time of writing it names 1,678 files, 1,000 in `sources-001` and
678 in `sources-002`. A line looks like this:

```text
f7ef3ae8a22e521f289803fe93543eb64c329b58aa73a9e224dfd915a2a5f4f7 002 curl/curl-8.22.0.tar.xz
```

so that file's address is

```text
https://github.com/kunaldawn/kdos/releases/download/sources-002/f7ef3ae8a22e521f289803fe93543eb64c329b58aa73a9e224dfd915a2a5f4f7
```

The recipe hash, the asset's name and the digest GitHub computes for the asset are the same string,
so a file that verifies is the file the recipe meant, whichever route it came by. The archive and
the index are append-only: an asset whose digest matches its name is never replaced or deleted, so
an old checkout can still find the exact bytes it was written against after upstream has moved on or
gone. Each archive release's notes on GitHub list every file it holds, so a file can also be found
by browsing the Releases page. `ports/srclib.sh` holds this addressing, and `ports/fetch`,
`ports/publish` and the pre-push hook all read it from there, so the three cannot disagree about
where a hash lives.

`ports/fetch` resolves each file a `sha256 =` line names, other than files git tracks itself (such
as patches), by trying these locations in order and stopping at the first copy that verifies:

| | Location |
|---|---|
| 1 | The port directory: a file already there |
| 2 | The cache, `ports/.srccache/sha256-XX/<hash>` (`XX` is the hash's first two hex digits) |
| 3 | The archive, `$KDOS_SOURCES_BASE/sources-NNN/<hash>`, with `NNN` from `ports/sources.idx`; a hash the index does not name skips this step |
| 4 | The recipe's `source =` URL for that file |
| 5 | Generation, only for the port's own `<name>-vendor-<version>.tar.xz` |

A copy that fails its hash, or is empty, is removed and the next location is tried. If the archive
cannot be reached, the fetch falls through to upstream. The archive is asked before upstream because
it does not disappear when an upstream host does.

**The cache.** Every copy that verifies is entered into the cache, and the port directory holds the
same file. Both are hard links when the cache and the checkout share a filesystem, so the cache
costs no extra space; otherwise each is a copy, so a cache on another disk holds a second copy of
every source. Either way, switching branches downloads nothing already fetched. A cache entry whose
bytes do not hash to its name is deleted rather than served. The cache holds each file under its
hash, split into `sha256-XX/` directories by the first two hex digits only to keep each directory
small. It is ignored by git and survives `make clean`; deleting it costs a full re-download and
nothing else. It belongs to one checkout. A second checkout or a fresh clone starts with an empty
cache unless `KDOS_SRCCACHE` points both at a shared path, or unless it is fetched with `ports/fetch
--tree <dir>` from a checkout that already has the cache.

**Unhashed files.** A file named by a recipe with no hash yet (for example, just after bumping
`version =`) is downloaded from upstream only, with a warning, and never from the archive or the
cache. `KDOS_ALLOW_UNVERIFIED=1` silences the warning. A vendor bundle with no hash is generated.

**Leftover archives.** Source files are not tracked by git, so a pull or branch switch that moves a
port to another version leaves the old file in the port directory. Nothing the build reads names
it; `testing/preflight.sh` lists it as a stale fetched archive, and it is safe to delete.

**Interrupted downloads.** A download goes to `<file>.part` and is renamed into place only once it
verifies. The next run resumes the partial file. If a resumed download fails its hash, it is fetched
once more from the beginning from the same URL before the next location is tried, because the
partial file may have held the start of different bytes.

**Vendor bundles.** Ports written in Rust, Go, Python and Haskell build offline from a *vendor
bundle*: a tarball of their dependencies, generated by the language's own package manager. For Rust,
Go and Haskell that toolchain is the version this tree compiles with; Python bundles are resolved by
the `python3` and `pip` of the fetch image's Debian release, not by the tree's own Python. A recipe
asks for one with `vendoring =` (`rust`, `go`, `node`, `haskell` or `python`) or with `pypackages
=`. In the current tree 71 recipes use `rust`, 38 `go`, 45 `python`, 4 `haskell` and 1 `node`.
A default `make fetch` runs in two passes. Your machine resolves everything it
can from the first four locations with `curl` and `sha256sum`. Only a port whose vendor bundle is
still missing is handed to the `kdos-fetch` container, which generates it; a clone whose archive is
complete never builds that image. A port missing any other file is not handed over, because its
bundle cannot be generated without its source, and fails on your machine.

The `kdos-fetch` image is built from `ports/Containerfile.fetch` on Debian trixie, with the Rust,
Go, Node, GHC and cabal versions read from the `rust`, `go`, `nodejs`, `ghc` and `cabal-install`
recipes. It runs with host networking and as your own user and group, so everything it writes is
yours; its home directory and each toolchain's cache live in `build/fetch-home`. A generated bundle
is held to its recipe hash like a download. Bundles are built reproducibly (sorted names, a fixed
timestamp, owner 0, `xz -9 -T1`), so the same inputs give the same bytes.

| Command | Does |
|---|---|
| `ports/fetch [port…]` | Fetch every port, or only the named ones |
| `ports/fetch --check [port…]` | Offline: list each archived file that is missing or fails its hash, and each recipe that cannot be read; exit 1 if any. `make fetch-check` runs it over every port |
| `ports/fetch --tree <dir> [port…]` | Fetch for the `ports/core` of another checkout `<dir>`, using this tree's archive logic, recipe reader, cache and index. Never generates a vendor bundle |

A checkout with no `ports/srclib.sh` cannot fetch from the archive by itself, and this tree's
`ports/fetch` cannot be copied into it, because it needs `srclib.sh` beside it. Fetch for it from a
checkout that has one instead:

```sh
ports/fetch --tree ../kdos-old
```

This works because the index only grows: the newest `ports/sources.idx` names every file an older
checkout's recipes can ask for. In this mode, a file the other checkout tracks through a Git LFS
filter counts as an archive to fetch, because without LFS its working copy is only a pointer.

| Variable | Default | Effect |
|---|---|---|
| `KDOS_SOURCES_REPO` | `kunaldawn/kdos` | The archive repository |
| `KDOS_SOURCES_BASE` | `https://github.com/$KDOS_SOURCES_REPO/releases/download` | The archive's download base. Set it empty to use upstream only |
| `KDOS_SOURCES_INDEX` | `ports/sources.idx` | The index to read |
| `KDOS_SRCCACHE` | `ports/.srccache` | The cache. A path outside the repository is mounted into the fetch container |
| `KDOS_FETCH_HOST` | unset | `1`: one pass on your machine, generating vendor bundles with its own toolchains |
| `KDOS_ALLOW_UNVERIFIED` | unset | `1`: no warning for a file its recipe gives no hash |
| `ENGINE_OUT` | `docker` if installed, else `podman` | The container engine for generating vendor bundles |
| `CC` | `cc` | The compiler for the recipe reader, `ports/.kpkgbin/kpkg` |

### The pre-push check

The `pre-push` hook enforces the archive's side of this. For each ref pushed, it takes the hashes
the pushed commit's recipes name and the remote's current commit does not, and requires each to be
listed in `ports/sources.idx` as of the pushed commit and present at the archive address that line
gives. A file git itself carries, such as a patch, is not checked. The hook also refuses the push
when it cannot reach the archive, or the archive answers anything other than 200 or 404, since it
then cannot prove the sources are there. Set `KDOS_SKIP_PUBLISH_CHECK=1` for a push you know is
safe. Setting `core.hooksPath` replaces `.git/hooks` entirely, including any hooks git-lfs
installed. How a new source reaches the archive is in
[Publishing sources](writing-ports.md#publishing-sources).

## Rebuilding one thing

A full build takes hours, and almost no change needs one. Find what you changed in this table:

| Changed | Run |
|---|---|
| Something under `fs/` | `make build BUILD_ARGS="--phases 01_phase1,06_packaging --steps 01_phase1:00_file_system.sh"` |
| One port's recipe | `make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild <port>"` |
| A desktop program | `make build BUILD_ARGS="--phases 05_desktop --rebuild <port>"` |
| A library under `src/libs/` | `make build BUILD_ARGS="--phases 01_phase1,04_phase4,05_desktop,06_packaging --steps 01_phase1:12_kpkg.sh,01_phase1:13_kinstall.sh"`; see below |
| The installer (`src/packages/kdos-installer`, or `kdos-appbox`'s `catalogue.c`) | `make build BUILD_ARGS="--phases 01_phase1,06_packaging --steps 01_phase1:13_kinstall.sh"`; see below |
| Only packaging | `make build BUILD_ARGS="--phases 06_packaging"` |
| Nothing; resuming an interrupted run | `make build BUILD_ARGS="--continue-from 04_phase4"` |
| Every phase, on the existing tree, skipping the startup picker | `make build BUILD_ARGS="--fresh --no-snapshot"` |

Use the phase the port is listed in: a port named in `script/03_phase3/packages.txt` is rebuilt with
`--phases 03_phase3,06_packaging`. Add `06_packaging` whenever you want a new ISO from the change.
[The ports catalogue](../06-reference/ports-catalogue.md) lists every port by phase and group. To
find the phase from the tree:

```sh
grep -l '^<port>$' script/*/packages.txt     # the phase(s) that name it
ls build/logs/*/*_<port>.install.log      # the phase whose logs hold its install
```

38 ports are named in more than one list: 8 phase-2 bootstrap ports are named again in `03_phase3`
(2 of them in `04_phase4` as well), 29 phase-3 ports again in `04_phase4`, and `xcb-util-wm` in both
`04_phase4` and `05_desktop`. zlib, gcc, binutils and bash are among them. For these the first
command prints several files. Pass only one of those phases: `--rebuild` forces the port in every
selected phase whose resolved install order contains it, as a listed port or as a dependency, so
selecting two such phases compiles it twice. The latest phase that names it is the usual choice,
because a port rebuilt under a phase is built with that phase's environment file, and the latest one
is the environment it was last built with. The files differ: `script/phase2.env.sh` names no
compiler, while `phase3.env.sh` onwards set `CC=gcc` and `CXX=g++`, and `src/packages` is on
`PORT_REPO` only from `phase4.env.sh` on (`phase4`, `phase5` and `desktop`).

An eighth of the ports in `ports/core` are named in no `packages.txt` (250 of 2,003); almost all of
them are installed because a listed port depends on them. For those, the second command finds the
phase, or use the phase of the first listed port that depends on it.

**Editing a library rebuilds every port of KDOS's own**, not only the ones that use it. KDOS's own
programs are recipes under `src/packages/` and `src/desktop/`, 24 in all. Such a recipe has no
upstream tarball, so its recipe hash covers its own directory and the whole of `src/libs`; working
out which libraries a `build.sh` actually compiles would need a shell parser inside the package
manager. Each of these ports takes seconds to compile. Upstream ports are unaffected. The phase
lists install 23 of the 24, in `04_phase4` and `05_desktop`, and their changed hash is enough to
rebuild them, so they need no `--rebuild`. The 24th, `kdos-installer`, is in no phase list, so
`--rebuild kdos-installer` reaches no phase and rebuilds nothing; the installer on the image comes
from the phase-1 step `13_kinstall.sh` instead, which also compiles in `catalogue.c` from
`src/packages/kdos-appbox`. An edit to that file rebuilds the `kdos-appbox` port through its
directory hash but leaves `kinstall` as it was until the step is named, as in the installer row
above.

Three programs link the libraries outside any recipe. The orchestrator is recompiled at the start of
every build. The package manager `kpkg` and the installer `kinstall` are compiled by the phase-1
steps `12_kpkg.sh` and `13_kinstall.sh`, which is why the command above names them. `12_kpkg.sh`
recompiles whenever its sources' hash differs from the one it recorded; `13_kinstall.sh` exits at
once when its completion marker under `build/mark/` exists, unless the
[build plan](build-system.md#build-plans) names it; that section also explains what a plan
suppresses and why only the named ports are forced. Leave the `01_phase1` part out when your change
touches neither program's libraries.

For a desktop program there is a faster loop still: `testing/quick.sh` builds only the named ports
into `build/fs`, with no packaging, and patches the files they own into the RAM overlay of a booted
ISO. It cannot carry anything under `fs/`, a new port, or a program that owns a D-Bus name, and what
it shows is not evidence about the shipped image. See [Testing](testing.md#the-fast-loop).

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
- **Do not re-run an early phase on a tree that is already past it while snapshots are being
  written**, as they are by `--fresh`, by `--continue-from` and by a plan run with `--snapshot`. The
  early phase's snapshot would be overwritten with the later tree, filed under its name. A
  [build plan](build-system.md#build-plans) that narrows the run with `--phases` or `--steps`, as
  the commands in [Rebuilding one thing](#rebuilding-one-thing) do, writes no snapshots and is safe;
  `--rebuild` on its own does not narrow, so it keeps them. To resume an interrupted run, use
  `--continue-from` with the phase it stopped in, not an earlier one.
- **Do not switch between running the `ports/` helpers in a container and on your machine without
  clearing them.** A helper compiled against the other C library fails without saying why; see
  [Where the build puts things](#where-the-build-puts-things) for which ones recover by themselves.
- **Run long builds where they will survive your shell.** A build started from a shell that exits
  dies with it; use `tmux`, `screen` or similar.

## Working without a build at all

Much of the system can be checked on your development machine, with no container and no virtual
machine:

```sh
testing/preflight.sh                              # the wiring: a few minutes
testing/selftest.sh                               # libraries and their consumers: a few minutes
make snapshots                                    # compiles build/.kdosbuild for this machine
build/.kdosbuild --preview build 132x43 vt        # a build screen, drawn as text
```

`testing/preflight.sh` checks the wiring a full build would otherwise fail on: that every package a
`packages.txt` names has a port, that every recipe parses, that the phase scripts are valid shell,
and similar. `testing/selftest.sh` compiles `kdosbuild`, `kinstall` and the other programs it tests
into a temporary directory, which it deletes when it exits, and runs their assertions. Among other
things it resolves the installer's plan over the shipped catalogue (`kinstall --dump plan`) and,
where the Wayland libraries are installed, draws the resource monitor's pages as text from recorded
`/proc` trees (`kdos-res --fixture testing/fixtures/res --dump`). Neither program is installed on a
development machine, so those forms are what the self-test runs, not commands to type. On one
development machine each script took about two and a half minutes. What each harness proves is in
[Testing](testing.md#what-each-tool-proves).

`build/.kdosbuild` runs on your machine only when your machine's compiler built it, which
`make snapshots` does. `make build` replaces it with a musl binary compiled in the build image, which
a glibc machine cannot run; the shell then reports `No such file or directory` without mentioning the
missing musl loader. Run `make snapshots` again after a build, or keep a separate copy with
`KDOSBUILD_BIN=build/.kdosbuild-host script/kdosbuild.sh --list`. `--preview` draws one of the
screens `build`, `activity`, `failure`, `pinned`, `complete`, `startup`, `plan` or `packages` at
the given size, in one of the [glyph tiers](../06-reference/glossary.md) `rich`, `vt` or `ascii`
(which set of drawing characters the screen may use).

The window model needs no display at all. `libkwm` links nothing but the C library, so the self-test
replays `testing/fixtures/wm/geometry.txt` against it on any machine in milliseconds. It is the
fastest way to answer a question about where a window lands, what a tiled window becomes, or which
edge a moving edge stops against.

## When a build fails

Work through these in order:

1. The failing step's log, under `build/logs/<phase>/`. The orchestrator names it; in the panel it
   opens when a step fails, `O` opens the log and `C` copies its path (see
   [Keys](build-system.md#keys)).
2. [Build troubleshooting](build-troubleshooting.md), which lists the failures that recur, by
   symptom. Check it before debugging from scratch.
3. The port's own `config.log`, if a configure script failed. `kpkg` leaves a failed port's
   unpacked source in place at `build/fs/var/cache/kpkg/work/<name>/<name>-<version>/`. `build/fs`
   is owned by root, so read it from a container or as root. `C compiler cannot create executables`
   rarely means what it says; the real error is in the log, next to the test program that failed.
4. `testing/preflight.sh`, which catches wiring mistakes (a missing dependency, an unlisted port)
   before a build that runs for hours finds them.

## Cutting a release

A KDOS release is three things: a git tag, an ISO, and a list of every source hash the tag's
recipes name. The list is one file that says what the release needs to build; the hashes themselves
are pinned by the tag, whose recipes git cannot change without changing the tag.

```sh
git tag <tag> && git push origin <tag>
git switch --detach <tag>                 # build from the tag, not the working tree
make fetch && make build
# create a DRAFT release <tag> on kunaldawn/kdos, attach build/iso-build/kdos.iso
ports/publish --freeze <tag>
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
`--dry-run` or `--check` it stops after step 3 and says what it would attach. Uploading needs a
token in `$KDOS_SOURCES_TOKEN` or `~/.config/kdos/sources-token`, which is refused when anyone but
you can read it; see [Publishing sources](writing-ports.md#publishing-sources).

The release starts as a draft so the ISO and the list go out together when you publish it. Leave
GitHub's immutable releases **off** on `kunaldawn/kdos`: the source archive's `sources-NNN`
releases live in the same repository, the setting applies to all of them, and it would freeze each
at its first publication, after which no new source could be added to it.

## See also

- [How KDOS is built](how-kdos-is-built.md): the whole build as one story, from clone to ISO
- [How KDOS differs](../01-philosophy/how-kdos-differs.md): why the sources are pinned and the
  build is self-hosting, compared with other distributions
- [The ports catalogue](../06-reference/ports-catalogue.md): every port, by phase and group
- [The build system](build-system.md): phases, snapshots, build plans, the chroot
- [Writing ports](writing-ports.md): adding or changing a recipe, and publishing its sources
- [Build troubleshooting](build-troubleshooting.md): recurring failures, by symptom
- [Testing](testing.md): what to run before believing a change
- [The C libraries](c-libraries.md): what KDOS programs are built on
- [Getting started](../02-user-guide/getting-started.md): the same build, from a user's side

<!-- book-nav -->
---

*Part V — Building and developing, chapter 31.* Previous: [30. How KDOS is built](how-kdos-is-built.md) · [Contents](../README.md) · Next: [32. The build system](build-system.md)
