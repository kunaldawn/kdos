# Repository layout

This page is a map of the KDOS source tree: what each top-level directory holds, which directories
are port repositories, what git ignores, and which files at the root matter. It is for anyone who has
cloned the repository and wants to find where something lives, whether to change it, build it or
just read it.

If you want the layout of a *running* KDOS system rather than of the source tree, read
[Filesystem and IPC](filesystem-and-ipc.md) instead. If you are about to build, read
[Developing](../05-developer/developing.md) first; this page tells you where things are, and that
one tells you what to run. Terms such as *port*, *box*, *pack* and *rig* are defined in the
[Glossary](glossary.md).

## The tree

```
kdos/
├── README.md              the front door
├── CLAUDE.md              rules and workflow for coding agents working on this tree
├── Makefile               every build and run target
├── Dockerfile             the build container
├── .dockerignore          what the build container's context leaves out
├── LICENSE
├── kdos.png, kdos.xcf     the mascot, from which the logo and marks are generated
│
├── docs/
│   ├── kdos/              this book
│   └── screenshots/       every image the documentation references
│
├── ports/
│   ├── core/<name>/       one directory per upstream port
│   │   ├── kpkgbuild          declarative metadata — parsed, never sourced
│   │   ├── build.sh           the build; bash, run in the unpacked source
│   │   ├── postinstall.sh     optional install-time hook
│   │   ├── *.patch            optional patches, tracked
│   │   └── <name>-<ver>.tar.* the upstream archive or vendor bundle — fetched, not tracked
│   ├── Containerfile.fetch    the image that generates vendor bundles, pinning this tree's toolchains
│   ├── hackage-vendor         the Hackage downloader behind `vendoring = haskell`
│   ├── .srccache/             the local source cache, laid out like the archive — ignored
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
│   │   ├── libkicon/          a name becomes a sprite slot, or −1
│   │   ├── libkimg/           the only place untrusted image bytes are decoded
│   │   ├── libkpack/          the pack format
│   │   ├── libkpkg/           the package database, the ports tree, the solver
│   │   ├── libkproc/          every reading about the machine, from a movable root
│   │   ├── libksig/           Ed25519 — Monocypher, the one third-party source carried unmodified
│   │   ├── libktui/           the terminal toolkit: cell buffer, widgets, charts
│   │   ├── libkvt/            the terminal state machine — forked from libtsm, maintained here
│   │   ├── libkwl/            the toolkit's Wayland backend
│   │   ├── libkwm/            the window model, taken out of the compositor that obeys it
│   │   ├── libkxdg/           desktop entries and the MIME cache
│   │   └── selftest.c         the shared assertion program
│   │
│   ├── desktop/           the desktop — a port repository, 13 recipes
│   │   ├── kdos-comp/         the compositor; KDOS additions in src/kdos-*.c
│   │   ├── kdos-shell/        the panel and every other surface: one binary under 53 names
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
│   ├── packages/          our own software that is not the desktop — a port repository, 11 recipes
│   │   ├── kdos-kpkg/         the package manager, under five names (no kpkgbuild)
│   │   ├── kdos-installer/    kinstall, the installer; compiles three libraries in
│   │   ├── kdos-appbox/       launching boxed applications, box management, the store
│   │   │   └── catalogue          every installable application, as a chain of apt packages
│   │   ├── kdos-boxinit/      process 1 inside a box; statically linked
│   │   ├── kdos-pack/         build, sign, index and diff packs
│   │   ├── kdos-tools/        the kdos command, the ksvc supervisor, and their siblings
│   │   ├── kdos-theme/        the stylesheet, icon and cursor generators
│   │   ├── kdos-splash/       the boot splash
│   │   ├── kdos-bb/           the forked ASCII-art demo
│   │   ├── kdos-icons/        vendored Papirus icons, pruned and recoloured
│   │   ├── kdos-cursors/      vendored Bibata cursors, pruned and recoloured
│   │   └── kdos-gtk-theme/    the vendored adw-gtk3 stylesheet
│   │
│   ├── build/kdosbuild/   the build orchestrator — runs on the build host only
│   └── tools/kdos-portup/ the upstream version checker — runs on the build host only
│
├── fs/                    copied verbatim into the target root filesystem
│   ├── etc/                   os-release (the version string), issue, motd;
│   │                          the accounts: passwd, group, shadow, shells, subuid, subgid;
│   │                          the network identity: hostname, hosts, resolv.conf, nsswitch.conf;
│   │                          inittab, init.d, fstab, profile and profile.d, bash.bashrc, skel,
│   │                          sysctl.conf, nftables.conf and nftables.d, modprobe.d,
│   │                          modules-load.d, udev, vtrgb, ld-musl-x86_64.path, polkit-1,
│   │                          pipewire, alsa, xdg, and kdos/ (login.conf, packd.conf, menu.conf,
│   │                          zram.conf, timers.d, keys)
│   ├── root/                  root's own shell dotfiles
│   └── usr/                   local/{bin,sbin,lib} — including the per-application launchers —
│                              and share/{kdos,applications,backgrounds,bash-completion,
│                              dbus-1,xdg-desktop-portal}; boot artwork is in share/kdos/boot
│
├── script/                the build
│   ├── 00_toolchain/ … 06_packaging/    the eight phases
│   ├── util/                             step helpers: port.sh, psf2limine.py
│   ├── *.env.sh                          per-phase environment and metadata
│   ├── kdosbuild.sh                      compiles and runs the orchestrator
│   ├── chroot_exec.sh, chroot_enter.sh   the chroot
│   └── hooks/pre-push                    refuses a push naming an unarchived source (opt-in)
│
├── script-mobile/         the aarch64 phase tree — seven environment files, an
│                          orchestrator and one port helper, and no phase steps
│
├── testing/
│   ├── preflight.sh          the wiring, in seconds
│   ├── selftest.sh           the libraries and their consumers
│   ├── docscheck.sh          the book: dead links, history, the page contract
│   ├── barcheck.c, boxcheck.c   checks selftest.sh compiles and runs
│   ├── hostcheck.sh          what a bare host can and cannot run
│   ├── fixtures/             recorded system state, 38 directories
│   ├── goldens/              committed reference frames, 193 files
│   ├── vnc-shot.py           drive and photograph a real session
│   ├── rig-image.sh          builds kdos-qemu-py, the image vnc-shot.py runs in
│   ├── quick.sh, quickpatch.sh   one port, patched into a booted ISO's RAM overlay
│   ├── qemu-audio.sh         the emulator's audio flags, shared by make run and the rig
│   ├── bios-boot.sh          boot with no UEFI firmware
│   ├── bootcheck/            the boot path, driven over a serial console
│   ├── qemu-hw/              the containerised emulator with accelerated graphics
│   ├── devdeps-image.sh      builds kdos-devdeps, where nothing in selftest skips
│   ├── Dockerfile.base, Dockerfile.qemu, Dockerfile.devdeps   what those images are
│   ├── install-to-disk.sh    run the installer into a disk image
│   ├── packlane.sh           the application lane on a booted machine
│   ├── appsweep.sh, appbox-smoke.sh, appreport.sh   launch catalogue applications and report
│   ├── oomd-fire.sh          make the memory-pressure killer fire in a guest
│   ├── usability.sh, usability.md   drive the desktop the way a person does, and its checklist
│   ├── prepare_base.py, test_runner.py, mini_build.py, report_gen.py
│   │                         build a minimal root filesystem image in build_test/ and
│   │                         build individual ports against it
│   └── notes/                recorded measurements
│
└── build/                 generated — ignored in its entirety
```

## The three port repositories

A *port* is one recipe: a `kpkgbuild` metadata file and a `build.sh` beside it (see
[Writing ports](../05-developer/writing-ports.md)). Three directories hold ports, and all three use
the same recipe format.

| Directory | Holds | Recipes | What decides a port goes here |
|---|---|---|---|
| `ports/core/` | Upstream software | 1,014 | It is somebody else's source |
| `src/packages/` | Our own software that is not the desktop | 11 | It is ours, and it is not a desktop component |
| `src/desktop/` | The desktop | 13 | It is ours, and it draws or serves the session |

Because the format is shared, building the desktop is not a special case anywhere in the build
system. The phase environment files name the three directories in search order.

`src/libs/`, `src/build/` and `src/tools/` are not port repositories. The libraries are compiled
into each program by that program's own recipe. The two tools run only on the build host and are
compiled on demand: `script/kdosbuild.sh` builds the orchestrator into `build/.kdosbuild`, and
`ports/update` builds the version checker into `ports/.portup`.

`src/packages/kdos-kpkg/` is the one directory under `src/packages/` without a `kpkgbuild`. The
package manager cannot be built by the package manager, so `script/01_phase1/12_kpkg.sh` compiles it
directly and installs it as `/usr/bin/kpkg` with four symlinks beside it: `kpkgadd`, `kpkgbuild`,
`kpkgdel` and `kpkgdepends`.

## The library rule

No library is built as an archive of its own. Each program's recipe compiles the library sources it
needs straight into its binary.

The libraries a phase-1 program needs link nothing but the C library. Phase 1 runs before any other
library exists on the target, and two programs are built in it:

| Program | Compiles in | Links |
|---|---|---|
| `kinstall`, the installer | `libkbase`, `libktui`, `libkcolor` | the C library only |
| `kpkg`, the package manager | `libkbase`, `libkpkg`, `libksig` (with Monocypher) | the C library only |

If any of those five libraries gained a real link dependency, the program that compiles it in would
have to move to a later phase with it — and the package manager is what every later phase uses.

Five libraries do link beyond the C library. Each is its own source directory, so a program that does
not draw pixels never compiles it in or links its dependencies:

| Library | Links |
|---|---|
| `libkwl` | fcft, fontconfig, pixman, xkbcommon and the Wayland client libraries |
| `libkcell` | fcft and pixman |
| `libkicon` | pixman and libpng |
| `libkimg` | pixman, plus libpng, libjpeg, libwebp, libsixel and libnsgif where present |
| `libkchrome` | pixman directly, plus everything `libkcell`, `libkicon` and `libkwl` link, since it is built on them |

The per-library dependencies are in [The C libraries](../05-developer/c-libraries.md#the-set).

## Where the upstream sources are

Upstream source archives are not stored in git. Each one is a release asset in the
`kunaldawn/kdos-sources` repository, named by its own SHA-256 hash, and `make fetch` downloads them
into the port directories. It is the only build step that uses the network. The details —
the lookup order, the cache, the vendor bundles and the environment variables — are in
[Developing](../05-developer/developing.md#where-sources-come-from).

Four files in the tree make this work:

| File | Does |
|---|---|
| `ports/srclib.sh` | The archive's addressing and hash checks, sourced by the other three |
| `ports/fetch` | Resolves every recipe hash from the port directory, `ports/.srccache/`, the archive or upstream, in that order, and generates a port's own vendor bundle when none of those holds it. `make fetch` runs it; `make fetch-check` runs `ports/fetch --check`, which is offline |
| `ports/publish` | Uploads sources the archive does not hold yet (needs a token); `--freeze <tag>` attaches a release's frozen `sources.sha256` list. See [Writing ports](../05-developer/writing-ports.md#publishing-sources) |
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

Setting `core.hooksPath` replaces `.git/hooks` as a whole, so any hook installed there — git-lfs's included —
stops running in that clone.

## Generated and ignored

| Path | Ignored by git | Notes |
|---|---|---|
| `build/` | entirely | The root filesystem, logs, snapshots, the ISO, signing keys, frozen source lists, the bake's container store |
| `build-mobile/` | entirely | The mobile build root; firmware blobs land under it |
| `build_test/` | entirely | Where `testing/prepare_base.py` builds the minimal root filesystem that `testing/test_runner.py` builds ports against |
| `ports/core/*/*.tar`, `*.tar.*`, `*.tgz`, `*.tbz2`, `*.txz`, `*.zip`, `*.7z`, `*.part` | yes | Upstream archives, vendor bundles and partial downloads, put there by `make fetch`. A patch or configuration file a recipe hashes is tracked and unaffected, and the archive fixtures under `testing/fixtures/` stay tracked |
| `ports/.srccache/` | yes | The source cache, `sha256-XX/<hash>`. Plain data: it survives `make clean` and `make cleanbuild` |
| `ports/.kpkgbin/`, `.portup`, `.portup-tools/` | yes | Compiled host helpers. `.kpkgbin/kpkg` is the recipe reader `ports/fetch` and `ports/publish` use; `.portup` is the version checker and `.portup-tools/` its own recipe reader. `.gitignore` also lists `ports/.kpkg-meta`, which nothing in the tree builds |
| `ports/.update-cache.json` | yes | Per-port version-check results, kept for 24 hours |
| `docs/` except `docs/kdos/` and `docs/screenshots/` | yes | Room for scratch notes. A new documentation directory needs a line in `.gitignore`, or git will not see anything in it |
| `testing/logs/`, `testing/test_results.json` | yes | Test-run output. `testing/fixtures/` is *not* ignored — those are the recorded inputs the self-test replays |
| `__pycache__/`, `.claude/`, `ports-archived/`, `port-archive.sh` | yes | Local scratch and tool state |

**Clear the three compiled helpers when you switch between building in a container and building on
your own machine.** A helper built against one C library (musl in the container, usually glibc on
the host) cannot run under the other, and the error does not say why. A container run as root leaves
them owned by root, so delete them from inside a container rather than with `sudo`.

## Files at the root

| File | Is |
|---|---|
| `README.md` | The front door: what KDOS is, and how to build and run it |
| `CLAUDE.md` | Rules and workflow for coding agents working on this tree. It is not a description of how the system works — that is this book |
| `Makefile` | Every target: `all` (the default, the same as `build`), `fetch`, `fetch-check`, `updates`, `build`, `check-iso-free`, `snapshots`, `run`, `run-hw`, `rundisk`, `rundisk-hw`, `debug-boot`, `check-hw`, `cleandisk`, `cleanbuild`, `clean`. `check-iso-free` runs before every `build` and stops it while another process — usually a running VM — holds `build/iso-build/kdos.iso` open, since rewriting the image under a guest gives it I/O errors; `ALLOW_ISO_IN_USE=1` overrides it. See [Developing](../05-developer/developing.md#make-targets) |
| `Dockerfile` | The build container |
| `.dockerignore` | What the build container's context leaves out: `build/` and `ports-archived/` |
| `.gitignore` | The ignore rules in the table above |
| `.gitattributes` | Declares that no path goes through a filter |
| `LICENSE` | The licence |
| `kdos.png`, `kdos.xcf` | The mascot |

The boot splash, the terminal banner logo and the icon marks all derive from the mascot, but none
of them is regenerated by the build, which reads only committed files:

| Artwork | Committed file | Made from `kdos.png` by |
|---|---|---|
| Boot splash penguin | `src/packages/kdos-splash/penguin.h` | Cropping and quantising the mascot; no script in the tree does it |
| Terminal banner logo | `fs/usr/share/kdos/logo.txt` | `src/packages/kdos-splash/genlogo.py`, which decodes `penguin.h` |
| Icon marks | `src/packages/kdos-icons/marks/` | `src/packages/kdos-icons/genmarks.py` (needs Python's PIL) |

After changing `kdos.png`, regenerate all three on the host and commit the results, or the artwork
and the mascot drift apart.

## What must never exist

`fs/etc/X11/`. There is no X server on this system. The one carve-out is Xwayland, a rootless X
server the compositor runs for X11 clients inside boxes, and it needs nothing in that directory. See
[Principles](../01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## See also

- [Architecture overview](../03-architecture/overview.md) — the three rings this tree implements
- [The build system](../05-developer/build-system.md) — how the phases traverse it
- [Writing ports](../05-developer/writing-ports.md) — the recipe format
- [Filesystem and IPC](filesystem-and-ipc.md) — the target's layout, not the source tree's
- [The programs](../04-programs/README.md) — what each source directory produces
