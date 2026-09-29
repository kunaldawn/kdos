# Repository layout

This chapter is a map of the KDOS source tree: what each top-level directory holds, which
directories are port repositories, how the C libraries are compiled into their consumers, how the
build container and the chroot see the tree, where the upstream sources live, what git ignores,
which files at the root matter, what the artwork derived from the mascot is made from, and the one
path that must never exist. It is for anyone who has cloned the repository and wants to find where
something lives, whether to change it, build it or read it.

For the layout of a *running* KDOS system rather than of the source tree, read
[Filesystem and IPC](filesystem-and-ipc.md). For the story of how these directories become a
bootable image, read [How KDOS is built](../05-developer/how-kdos-is-built.md) first; for the
commands to run, [Developing](../05-developer/developing.md). Every recipe under `ports/core/` is
listed by phase and group in [The ports catalogue](ports-catalogue.md). Terms such as *port*,
*box*, *pack*, *lane* and *rig* are defined in the [Glossary](glossary.md).

## The tree

The top-level directories divide by role. `src/` holds KDOS's own source: the C libraries, the
desktop and the other programs written for KDOS. `ports/` holds the recipes for upstream software
and the tools that fetch its sources. `fs/` holds the files copied as they are into the target's
root filesystem. `script/` is the build, phase by phase; `testing/` is the tests and the test rig;
`build/` is where the build writes, and git ignores it. Three directories beneath `ports/` and
`src/` hold recipes; [The three port repositories](#the-three-port-repositories) explains which,
and how they map onto the three rings of the running system.

```
kdos/
├── README.md              the front door
├── CLAUDE.md              the contributor briefing read by coding assistants
├── Makefile               every build and run target
├── Dockerfile             the build container
├── .dockerignore          what the build container's context leaves out
├── LICENSE
├── kdos.png, kdos.xcf     the mascot, from which the splash, logos and marks derive
│
├── docs/
│   ├── kdos/              this book
│   └── screenshots/       every image the documentation references
│
├── ports/
│   ├── core/<name>/       one directory per upstream port
│   │   ├── kpkgbuild          declarative metadata — parsed, never sourced
│   │   ├── build.sh           the build; bash, run in the unpacked source
│   │   ├── postinstall.sh     optional install-time hook (24 ports have one)
│   │   ├── *.patch            optional patches, tracked (425 files)
│   │   ├── other support files  configuration, data files, single-file sources — tracked
│   │   └── <name>-<ver>.tar.* the upstream archive or vendor bundle — fetched, not tracked
│   ├── Containerfile.fetch    the image that generates vendor bundles, pinning this tree's toolchains
│   ├── hackage-vendor         the Hackage downloader behind `vendoring = haskell`
│   ├── .srccache/             the local source cache, one file per hash — ignored
│   ├── sources.idx            which source-archive release holds each hash — committed
│   ├── .kpkgbin/, .portup, .portup-tools/   host helpers compiled on demand — ignored
│   ├── srclib.sh              the source archive's addressing, shared by fetch, publish and the hook
│   ├── fetch                  download and verify sources; generate vendor bundles (`make fetch`)
│   ├── publish                upload sources to the archive; freeze a release's hash list
│   └── update                 front end of the upstream version checker (`make updates`)
│
├── src/
│   ├── libs/              the C libraries, compiled by their consumers — see the rule below
│   │   ├── libkbase/          allocation, strings, files, processes, the trash
│   │   ├── libkbuild/         phases, plans, the snapshot inventory
│   │   ├── libkcell/          the glyph cache and the cell painter
│   │   ├── libkchrome/        the window furniture
│   │   ├── libkcolor/         the palette table and colour arithmetic
│   │   ├── libkdisp/          which display server, and the surface lifecycle
│   │   ├── libkicon/          icon lookup: resolves an icon name or file to a sprite
│   │   ├── libkimg/           the only place untrusted image bytes are decoded
│   │   ├── libkpack/          the pack format
│   │   ├── libkpkg/           the package database, the ports tree, the solver
│   │   ├── libkproc/          reads system state from /proc and /sys, or from a fixture root
│   │   ├── libksig/           Ed25519 — Monocypher, the one third-party source carried unmodified
│   │   ├── libktui/           the terminal toolkit: cell buffer, widgets, charts, motion
│   │   ├── libkvt/            the terminal state machine — forked from libtsm, maintained here
│   │   ├── libkwl/            the toolkit's Wayland backend
│   │   ├── libkwm/            the window model, taken out of the compositor that obeys it
│   │   ├── libkxdg/           desktop entries and the MIME cache
│   │   └── selftest.c         the shared assertion program
│   │
│   ├── desktop/           the desktop — a port repository, 13 recipes
│   │   ├── kdos-comp/         the compositor; KDOS additions in src/kdos-*.c
│   │   ├── kdos-shell/        the panel and every other surface: one binary under 55 names
│   │   ├── kdos-res/          the resource monitor, and its setuid helper kdos-resctl
│   │   ├── kdos-lock/         the lock screen, and the setuid password checker kdos-checkpass
│   │   ├── kdos-term/         the terminal
│   │   ├── kdos-record/       the screen recorder
│   │   ├── kdos-boxsock/      one tagged compositor socket per box
│   │   ├── kdos-powerd/       suspend, poweroff, reboot; client kdos-power
│   │   ├── kdos-energyd/      per-application energy attribution; client kdos-energy
│   │   ├── kdos-oomd/         memory-pressure protection
│   │   ├── kdos-mountd/       removable media, LUKS, SMB; client kdos-mount
│   │   ├── kdos-packd/        the only thing that mounts a pack
│   │   └── xdg-desktop-portal-kdos/  the file chooser, settings, app chooser and access portals
│   │
│   ├── packages/          KDOS's own software that is not the desktop — a port repository, 11 recipes
│   │   ├── kdos-kpkg/         the package manager, under five names (no kpkgbuild)
│   │   ├── kdos-installer/    kinstall, the installer; built directly in phase 1
│   │   ├── kdos-appbox/       launching boxed applications, box management, the store
│   │   │   └── catalogue          every installable application, as a chain of apt packages
│   │   ├── kdos-boxinit/      process 1 inside a box; statically linked
│   │   ├── kdos-pack/         build, sign, index and diff packs
│   │   ├── kdos-tools/        one binary: the kdos command, the ksvc supervisor, kdos-getty,
│   │   │                      kdos-bootctl and the other small system tools
│   │   ├── kdos-theme/        the stylesheet, icon and cursor generators
│   │   ├── kdos-splash/       the boot splash, and the host-only boot-artwork generators
│   │   ├── kdos-bb/           the ASCII-art demo, a fork of the AA-project's bb
│   │   ├── kdos-icons/        vendored Papirus icons, pruned and recoloured
│   │   ├── kdos-cursors/      vendored Bibata cursors, pruned and recoloured
│   │   └── kdos-gtk-theme/    the vendored adw-gtk3 stylesheet
│   │
│   ├── build/kdosbuild/   the build orchestrator — runs on the build host only
│   └── tools/kdos-portup/ the upstream version checker — runs on the build host only
│
├── fs/                    copied verbatim into the target root filesystem
│   ├── etc/                   the system configuration; see the table below the tree
│   ├── root/                  root's own shell dotfiles
│   └── usr/                   local/{bin,sbin,lib} — including the per-application launchers —
│                              and share/{kdos,applications,backgrounds,bash-completion,
│                              dbus-1,xdg-desktop-portal}; boot artwork is in share/kdos/boot,
│                              the help pages in share/kdos/doc
│
├── script/                the build
│   ├── 00_toolchain/ … 06_packaging/    the eight phases: step scripts, or a packages.txt
│   ├── util/                             step helpers: port.sh, psf2limine.py
│   ├── *.env.sh                          per-phase environment and metadata
│   ├── kdosbuild.sh                      compiles and runs the orchestrator
│   ├── chroot_exec.sh, chroot_enter.sh   the chroot
│   └── hooks/pre-push                    refuses a push naming an unarchived source (opt-in)
│
├── testing/
│   ├── preflight.sh          compiles kpkg, then checks the tree's wiring (packages resolve,
│   │                         recipes parse, scripts are valid) without building a port
│   ├── selftest.sh           the libraries and their consumers
│   ├── docscheck.sh          the book: dead links, history, the page contract
│   ├── barcheck.c, boxcheck.c   checks selftest.sh compiles and runs
│   ├── hostcheck.sh          in a booted guest: is the binary each name resolves to the right one
│   ├── fixtures/             recorded system state, 40 directories
│   ├── goldens/              committed reference frames: 202 files and a README
│   ├── vnc-shot.py           drive and photograph a real session
│   ├── rig-image.sh          builds kdos-qemu-py, the image vnc-shot.py runs in
│   ├── quick.sh, quickpatch.sh   one port, patched into a booted ISO's RAM overlay
│   ├── qemu-audio.sh         the emulator's audio flags, shared by make run and the rig
│   ├── bios-boot.sh          boot with no UEFI firmware
│   ├── bootcheck/            the boot path, driven over a serial console
│   ├── qemu-hw/              the containerised emulator with accelerated graphics
│   ├── devdeps-image.sh      builds kdos-devdeps, where nothing in selftest skips
│   ├── Dockerfile.qemu, Dockerfile.devdeps   what those two images are
│   ├── install-to-disk.sh    run the installer into a disk image
│   ├── packlane.sh           exercise the pack lane (installing a signed pack into a box)
│   │                         on a booted machine
│   ├── appsweep.sh, appbox-smoke.sh, appreport.sh   launch catalogue applications and report
│   ├── oomd-fire.sh          make the memory-pressure killer fire in a guest
│   ├── usability.sh, usability.md   drive the desktop the way a person does, and its checklist
│   ├── prepare_base.py, test_runner.py, mini_build.py, report_gen.py, Dockerfile.base
│   │                         build a minimal root filesystem image in build_test/ and
│   │                         build individual ports against it, in the kdos-base-test
│   │                         image that Dockerfile.base describes
│   └── notes/                recorded measurements
│
└── build/                 generated — ignored in its entirety
```

`fs/etc/` holds, by concern:

| Concern | Files under `fs/etc/` |
|---|---|
| Identity and login text | `os-release` (the version string), `issue`, `motd` |
| Accounts | `passwd`, `group`, `shadow`, `shells`, `subuid`, `subgid` |
| Network | `hostname`, `hosts`, `resolv.conf`, `nsswitch.conf`, `nftables.conf`, `nftables.d/` |
| Init and shells | `inittab`, `init.d/`, `fstab`, `profile`, `profile.d/`, `bash.bashrc`, `skel/` |
| Kernel, modules and devices | `sysctl.conf`, `modprobe.d/`, `modules-load.d/`, `udev/`, `vtrgb`, `ld-musl-x86_64.path` |
| Desktop services | `polkit-1/`, `pipewire/`, `alsa/`, `xdg/` |
| KDOS's own | `kdos/`: `login.conf`, `packd.conf`, `menu.conf`, `zram.conf`, `timers.d/`, `keys/` |

The keys these files hold are listed in [Configuration](configuration.md).

## The three port repositories

A *port* is one recipe: a `kpkgbuild` metadata file and a `build.sh` beside it (see
[Writing ports](../05-developer/writing-ports.md)). Three directories hold ports, and all three use
the same recipe format.

The three port repositories, with `src/libs/`, carry the first two of the three *rings* described
in [Architecture overview](../03-architecture/overview.md#the-three-rings): `ports/core/` is the
core ring, and `src/desktop/` and `src/packages/`, together with the libraries in `src/libs/` that
they compile in, are the desktop ring. The outer ring, the
applications that run in boxes, is not a port repository; it is the catalogue file
`src/packages/kdos-appbox/catalogue`.

| Directory | Holds | Recipes | What decides a port goes here |
|---|---|---|---|
| `ports/core/` | Upstream software | 1,999 | It is somebody else's source |
| `src/packages/` | KDOS's own software that is not the desktop | 11 | It is written for KDOS, and it is not a desktop component |
| `src/desktop/` | The desktop | 13 | It is written for KDOS, and it draws or serves the session |

The counts are directories holding a `kpkgbuild`. Because the format is shared, building the
desktop is not a special case anywhere in the build system. The package manager finds a recipe
through `PORT_REPO`, an ordered search path; its default in `kpkg.conf` is `/ports/core`, and the
phase environment files widen it as the phases need more:

| Environment file | `PORT_REPO` |
|---|---|
| `script/phase2.env.sh`, `script/phase3.env.sh` | not set: the default, `/ports/core` |
| `script/phase4.env.sh`, `script/phase5.env.sh` | `/ports/core /kdos/src/packages` |
| `script/desktop.env.sh` | `/ports/core /kdos/src/packages /kdos/src/desktop` |

The paths are the chroot's view of the tree, described in
[How the build sees the tree](#how-the-build-sees-the-tree).

Which port is built in which phase is decided by the `packages.txt` in each phase directory
(`script/02_phase2/` to `script/05_phase5/`, and `script/05_desktop/`), not by the directory the
recipe lives in. Comment banners divide each list into named groups, such as "Core Services" or
"Modern CLI tools (Rust / Go)". [How KDOS is built](../05-developer/how-kdos-is-built.md) explains
the phases and [The ports catalogue](ports-catalogue.md) lists every port under its group.

`src/libs/`, `src/build/` and `src/tools/` are not port repositories. The libraries are compiled
into each program by that program's own recipe. The two tools run only on the build host and are
compiled on demand: `script/kdosbuild.sh` builds the orchestrator into `build/.kdosbuild`, and
`ports/update` builds the version checker into `ports/.portup`.

Two directories under `src/packages/` are built by phase-1 scripts rather than through a phase
list, because phase 1 runs before the package manager exists:

- `src/packages/kdos-kpkg/` has no `kpkgbuild`. The package manager cannot be built by the package
  manager, so `script/01_phase1/12_kpkg.sh` compiles it directly and installs it as `/usr/bin/kpkg`
  with four symlinks beside it: `kpkgadd`, `kpkgbuild`, `kpkgdel` and `kpkgdepends`.
- `src/packages/kdos-installer/` has a recipe, but no `packages.txt` names it.
  `script/01_phase1/13_kinstall.sh` compiles `kinstall` from the same directory, taking every `.c`
  file by glob so that the phase-1 build and the recipe's `build.sh` compile the same sources.

## The library rule

No library is built as an archive of its own. Each program's recipe compiles the library sources it
needs straight into its binary.

The libraries a phase-1 program needs link nothing but the C library. Phase 1 runs before any other
library exists on the target, and two programs are built in it:

| Program | Compiles in | Links |
|---|---|---|
| `kinstall`, the installer | `libkbase`, `libktui`, `libkcolor`, and `kdos-appbox`'s `catalogue.c` | the C library only |
| `kpkg`, the package manager | `libkbase`, `libkpkg`, `libksig` (with Monocypher) | the C library only |

If any of those five libraries gained a real link dependency, the program that compiles it in would
have to move to a later phase with it — and the package manager is what every later phase uses.

Five libraries do link beyond the C library. Each is its own source directory, so a program that does
not draw pixels never compiles it in or links its dependencies:

| Library | Links |
|---|---|
| `libkwl` | fcft, fontconfig, pixman, xkbcommon and the Wayland client libraries |
| `libkcell` | fcft, fontconfig and pixman |
| `libkicon` | pixman and libpng |
| `libkimg` | pixman, plus whichever of libpng, libjpeg, libwebp, libnsgif and libsixel the consumer enables with a `KIMG_HAVE_*` flag |
| `libkchrome` | pixman directly, plus everything `libkcell`, `libkicon` and `libkwl` link, since it is built on them |

The per-library dependencies are in [The C libraries](../05-developer/c-libraries.md#the-set).

## How the build sees the tree

`make build` does not run the build in the checkout directly. It builds the `os-dev` image from the
`Dockerfile` (Alpine, with a host compiler and the usual build tools) and runs it with no network,
mounting only the directories the build reads:

| Checkout path | In the build container | Mode |
|---|---|---|
| `build/` | `/workspace/build` | read-write |
| `src/`, `fs/`, `script/`, `ports/` | `/workspace/src`, `/workspace/fs`, `/workspace/script`, `/workspace/ports` | read-only |

`docs/`, `testing/` and the root files are not mounted, so nothing in them can
change what a build produces. The build writes only under `build/`; `build/fs/` is the target root
filesystem it grows phase by phase.

From phase 2 on, each port is built inside a chroot of `build/fs/`. `script/chroot_exec.sh`
bind-mounts the container's `/workspace` at `/kdos` inside it, with `build/`, `script/` and `src/`
mounted again beneath it, and `ports/` at `/ports`. That is why the phase environment files name
`/ports/core` and `/kdos/src/packages`: they are the same directories as the checkout's
`ports/core/` and `src/packages/`, seen from inside the chroot. The mechanism is in
[The build system](../05-developer/build-system.md).

A build run with `KDOS_ISO_SOURCES=1` has `script/06_packaging/02_iso.sh` copy `/kdos/src` and
`/kdos/script` onto the ISO under `sources/`. The step also copies `/kdos/ports`, but inside the
chroot that path is only the mount point the container left in `/workspace`: the bind of
`/workspace` at `/kdos` does not carry the container's separate `ports/` mount, and the ports tree
is mounted at `/ports` instead. `sources/ports/` on the medium is therefore empty, and the step's
removal of `.srccache/`, `.portup-tools/`, `.kpkg-meta` and `.update-cache.json` from it acts on
nothing. The step also names the `Makefile`, the `Dockerfile` and `CLAUDE.md`, which are not
mounted into the build and are skipped.

## Where the upstream sources are

Upstream source archives are not stored in git. A recipe names each of its files by its SHA-256
hash, and each archived file is a release asset of the `kunaldawn/kdos` repository, named by that
hash. `make fetch` puts every file in its port directory; it is the only build step that uses the
network. `ports/sources.idx` holds 1,678 entries, spread over the releases `sources-001` and
`sources-002`. The lookup order, the cache, the vendor bundles and the environment variables are
described in [Developing](../05-developer/developing.md#where-sources-come-from).

Five files in the tree make this work:

| File | Does |
|---|---|
| `ports/srclib.sh` | The archive's addressing and hash checks, and the on-demand build of the recipe reader; sourced by `ports/fetch`, `ports/publish` and the hook |
| `ports/sources.idx` | One line per archived file, `<sha256> <NNN> <port>/<file>`: the file is asset `<sha256>` of release `sources-<NNN>`. Written by `ports/publish`, read by `ports/fetch` and the pre-push hook; append-only, and committed with the recipe that needs it |
| `ports/fetch` | Resolves every recipe hash from the port directory, `ports/.srccache/`, the archive, or the recipe's `source =` URL, in that order, and generates a port's own vendor bundle when none of those holds it. `make fetch` runs it; `make fetch-check` runs `ports/fetch --check`, which is offline |
| `ports/publish` | Uploads sources the archive does not hold (needs a token) and writes their index lines; `--check` and `--dry-run` report without uploading; `--freeze <tag>` attaches a release's frozen `sources.sha256` list. See [Writing ports](../05-developer/writing-ports.md#publishing-sources) |
| `script/hooks/pre-push` | Refuses a `git push` whose recipes name a hash the archive does not hold |

The hook runs only in a clone that has opted in:

```sh
git config core.hooksPath script/hooks
```

With the hook on, a push is refused when the archive cannot be reached, when it answers anything but
200 or 404, or when `KDOS_SOURCES_BASE` is empty, because the hook cannot then rule out that a
source is missing. Working offline therefore means skipping the check for that push:

```sh
KDOS_SKIP_PUBLISH_CHECK=1 git push …
```

Setting `core.hooksPath` replaces `.git/hooks` as a whole, so any hook installed there, git-lfs's
included, stops running in that clone.

## Generated and ignored

| Path | Ignored by git | Notes |
|---|---|---|
| `build/` | entirely | The root filesystem, logs, snapshots, the ISO, signing keys, frozen source lists, and `build/podman/`, a podman container store that `script/kdosbuild.sh` leaves owned by root |
| `build_test/` | entirely | Where `testing/prepare_base.py` builds the minimal root filesystem that `testing/test_runner.py` builds ports against |
| `ports/core/*/*.tar`, `*.tar.*`, `*.tgz`, `*.tbz2`, `*.txz`, `*.zip`, `*.7z`, `*.part` | yes | Upstream archives, vendor bundles and partial downloads, put there by `make fetch`. A patch or configuration file a recipe hashes is tracked and unaffected, and the archive fixtures under `testing/fixtures/` stay tracked |
| `ports/.srccache/` | yes | The source cache, `sha256-XX/<hash>`. Plain data: it survives `make clean` and `make cleanbuild` |
| `ports/.kpkgbin/`, `.portup`, `.portup-tools/` | yes | Compiled host helpers. `.kpkgbin/kpkg` is the recipe reader `ports/fetch` and `ports/publish` use, built by `ports/srclib.sh`; `.portup` is the version checker and `.portup-tools/` its own recipe reader. `.gitignore` also lists `ports/.kpkg-meta`, which nothing in the tree builds |
| `ports/.update-cache.json` | yes | Per-port version-check results, kept for 24 hours |
| `docs/` except `docs/kdos/` and `docs/screenshots/` | yes | Room for scratch notes. A new documentation directory needs a line in `.gitignore`, or git will not see anything in it |
| `testing/logs/`, `testing/test_results.json` | yes | Test-run output. `testing/fixtures/` is *not* ignored — those are the recorded inputs the self-test replays |
| `__pycache__/`, `.claude/`, `ports-archived/`, `port-archive.sh` | yes | Local scratch and tool state |

The compiled helpers `ports/.kpkgbin/`, `ports/.portup` and `ports/.portup-tools/` are specific to
the C library they were built against: musl in the build container, usually glibc on a developer's
machine. A helper built against one cannot run under the other, and the error does not say why, so
they are deleted when switching between the two. A container run as root leaves them owned by root;
delete them from inside a container rather than with `sudo`.

## Files at the root

| File | Is |
|---|---|
| `README.md` | The front door: what KDOS is, and how to build and run it |
| `CLAUDE.md` | The contributor briefing read by coding assistants: hard rules, conventions and the build loop. It is not a description of how the system works; that is this book |
| `Makefile` | Every target: `all` (the default, the same as `build`), `fetch`, `fetch-check`, `updates`, `build`, `check-iso-free`, `snapshots`, `run`, `run-hw`, `rundisk`, `rundisk-hw`, `debug-boot`, `check-hw`, `cleandisk`, `cleanbuild`, `clean`. `check-iso-free` runs before every `build` and stops it while another process — usually a running VM — holds `build/iso-build/kdos.iso` open, since rewriting the image under a guest gives it I/O errors; `ALLOW_ISO_IN_USE=1` overrides it. See [Developing](../05-developer/developing.md#make-targets) |
| `Dockerfile` | The build container, `os-dev`: Alpine with the host tools the toolchain phase needs |
| `.dockerignore` | What the build container's context leaves out: `build/` and `ports-archived/` |
| `.gitignore` | The ignore rules in the table above |
| `.gitattributes` | Declares that no path goes through a filter |
| `LICENSE` | The licence |
| `kdos.png`, `kdos.xcf` | The mascot; `fs/usr/share/kdos/kdos.png` is a byte-identical copy shipped on the target |

Several pieces of artwork derive from the mascot. None of them is regenerated by the build, which
reads only committed files; each generator is a host-only Python script whose output is committed,
and it lives in a port directory rather than beside its output because `fs/` is copied verbatim
into the target, which carries no Python:

| Artwork | Committed file | Made from `kdos.png` by |
|---|---|---|
| Boot splash penguin | `src/packages/kdos-splash/penguin.h` | Cropping and quantising the mascot; no script in the tree does it |
| Terminal banner logo | `fs/usr/share/kdos/logo.txt` | `src/packages/kdos-splash/genlogo.py`, which decodes `penguin.h` |
| Icon marks | `src/packages/kdos-icons/marks/` | `src/packages/kdos-icons/genmarks.py` (needs Pillow) |
| The mascot in `kdos-bb` | `src/packages/kdos-bb/src/kdostux.c` | `src/packages/kdos-bb/genimg.py` (needs Pillow and a C compiler) |

Two more boot pictures contain the mascot without being generated from `kdos.png`.
`fs/usr/share/kdos/boot/kdos-banner.png` is committed as a picture; `genbanner.py` in
`src/packages/kdos-splash/` only repaints its two caption lines. `kdos-backdrop.png` beside it, the
boot menu's backdrop, is drawn from the banner by `genbackdrop.py` in the same directory.

After changing `kdos.png`, regenerate the four in the table on the host (and copy the mascot to
`fs/usr/share/kdos/kdos.png`) and commit the results, or the artwork and the mascot drift apart.

## What must never exist

`fs/etc/X11/`. There is no Xorg server on this system and nothing X on the login path. The one X
server is Xwayland, a rootless X server the compositor starts for X11 clients: boxed applications,
and host applications that have no Wayland path. It needs nothing in that directory. See
[Principles](../01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## See also

- [Architecture overview](../03-architecture/overview.md) — the three rings this tree implements
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — how these directories become an ISO
- [The build system](../05-developer/build-system.md) — how the phases traverse it
- [The ports catalogue](ports-catalogue.md) — every recipe under `ports/core/`, by phase and group
- [How KDOS differs](../01-philosophy/how-kdos-differs.md) — why sources are pinned by hash and
  everything is built from this tree
- [Writing ports](../05-developer/writing-ports.md) — the recipe format
- [Filesystem and IPC](filesystem-and-ipc.md) — the target's layout, not the source tree's
- [The programs](../04-programs/README.md) — what each source directory produces

<!-- book-nav -->
---

*Part VI — Reference, chapter 42.* Previous: [41. Filesystem and IPC](filesystem-and-ipc.md) · [Contents](../README.md) · Next: [43. Known gaps](known-gaps.md)
