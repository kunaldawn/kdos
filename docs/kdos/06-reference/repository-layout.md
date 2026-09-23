# Repository layout

The source tree, annotated. This is the map for finding where something lives. For the layout of a
*running* system, see [Filesystem and IPC](filesystem-and-ipc.md).

## The tree

```
kdos/
├── README.md              the front door
├── CLAUDE.md              rules, conventions and workflow for working on this tree
├── Makefile               every build and run target
├── Dockerfile             the build container
├── LICENSE
├── kdos.png, kdos.xcf     the mascot, from which the logo and marks are generated
├── plan.md                the implementation plan for work in progress
│
├── docs/
│   ├── kdos/              this book
│   └── screenshots/       every image the documentation references
│
├── ports/
│   ├── core/<name>/       one directory per upstream port
│   │   ├── kpkgbuild          declarative metadata — parsed, never sourced
│   │   ├── build.sh           the build; bash, working directory is the source
│   │   ├── postinstall.sh     optional install-time hook
│   │   ├── *.patch            optional
│   │   └── <name>-<ver>.tar.* upstream archive — in the tree, through Git LFS
│   ├── Containerfile.fetch    the fetch image, pinning this tree's toolchains
│   ├── fetch                  download and vendor sources
│   └── update                 the upstream version checker's front end
│
├── src/
│   ├── libs/              the C libraries — static, and see the rule below
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
│   │   ├── libkproc/          every reading, from a movable root
│   │   ├── libksig/           Ed25519 — the one vendored third-party source
│   │   ├── libktui/           the terminal toolkit: cell buffer, widgets, charts
│   │   ├── libkvt/            the terminal state machine — a fork of libtsm
│   │   ├── libkwl/            the toolkit's Wayland backend
│   │   ├── libkwm/            the window model, out of the compositor that obeys it
│   │   ├── libkxdg/           desktop entries and the mime cache
│   │   └── selftest.c         the shared assertion program
│   │
│   ├── desktop/           the desktop — a port repository, 13 recipes
│   │   ├── kdos-comp/         the compositor; KDOS additions in src/kdos-*.c
│   │   ├── kdos-shell/        one binary under 53 names
│   │   ├── kdos-res/          the resource monitor, and its setuid helper
│   │   ├── kdos-lock/         the lock screen, and the setuid password checker
│   │   ├── kdos-powerd/       suspend, poweroff, reboot
│   │   ├── kdos-energyd/      per-application energy attribution
│   │   ├── kdos-oomd/         memory-pressure protection
│   │   ├── kdos-mountd/       removable media
│   │   ├── kdos-packd/        the only thing that mounts a pack
│   │   ├── kdos-term/         the terminal
│   │   ├── kdos-record/       the desktop recorder
│   │   ├── kdos-boxsock/      one tagged compositor socket per box, per compositor
│   │   └── xdg-desktop-portal-kdos/  the file chooser, settings, app chooser
│   │
│   ├── packages/          ports that are ours — a port repository, 11 recipes
│   │   ├── kdos-kpkg/         the package manager, under five names
│   │   ├── kdos-installer/    the installer; links three libraries
│   │   ├── kdos-appbox/       launching boxed applications, box management, the store
│   │   │   └── catalogue          every application, as a chain of apt packages
│   │   ├── kdos-boxinit/      process 1 inside a box; statically linked
│   │   ├── kdos-pack/         build, sign, index and diff packs
│   │   ├── kdos-tools/        the kdos command, the supervisor, and their siblings
│   │   ├── kdos-theme/        the stylesheet, icon and cursor generators
│   │   ├── kdos-splash/       the boot splash
│   │   ├── kdos-bb/           the forked ASCII-art demo
│   │   ├── kdos-icons/        vendored, pruned, recoloured icons
│   │   ├── kdos-cursors/      vendored, pruned, recoloured cursors
│   │   └── kdos-gtk-theme/    the vendored stylesheet
│   │
│   ├── build/kdosbuild/   the build orchestrator — host only
│   └── tools/kdos-portup/ the upstream version checker — host only
│
├── fs/                    copied verbatim into the target root filesystem
│   ├── boot/                  boot artwork and loader configuration
│   ├── etc/                   inittab, fstab, nftables.conf, profile.d, init.d,
│   │                          skel, kdos, xdg, udev, polkit-1, pipewire, alsa
│   ├── root/                  root's own dotfiles
│   └── usr/                   local/{bin,sbin,lib} and share/{kdos,applications,
│                              backgrounds,bash-completion,dbus-1,
│                              xdg-desktop-portal}
│
├── script/                the build
│   ├── 00_toolchain/ … 06_packaging/    the eight phases
│   ├── util/                             shared step helpers
│   ├── *.env.sh                          per-phase environment and metadata
│   ├── kdosbuild.sh                      compiles and runs the orchestrator
│   └── chroot_exec.sh, chroot_enter.sh   the chroot
│
├── script-mobile/         the aarch64 phase tree — seven environment files, an
│                          orchestrator and one port helper, and no phase steps
│
├── testing/
│   ├── preflight.sh          the wiring, in seconds
│   ├── docscheck.sh          the book: dead links, history, the page contract
│   ├── selftest.sh           the libraries and their consumers
│   ├── hostcheck.sh          what a bare host can and cannot run
│   ├── quick.sh              one port, patched into a booted ISO's RAM overlay
│   ├── fixtures/             recorded system state, 38 directories
│   ├── goldens/              committed reference frames, 193 files
│   ├── vnc-shot.py           drive and photograph a real session
│   ├── rig-image.sh          builds kdos-qemu-py, the image vnc-shot.py runs in
│   ├── devdeps-image.sh      builds kdos-devdeps, where nothing in selftest skips
│   ├── Dockerfile.base, Dockerfile.qemu, Dockerfile.devdeps   what those images are
│   ├── packlane.sh           the application lane on a booted machine
│   ├── install-to-disk.sh    run the installer into a disk image
│   ├── appsweep.sh, appreport.sh   launch every application and report
│   ├── bootcheck/            the boot path, driven over a serial console
│   ├── qemu-hw/              the containerised emulator
│   └── notes/                recorded measurements
│
└── build/                 generated — gitignored in its entirety
```

## The three port repositories

Three directories hold ports, and they use one recipe format.

| Directory | Holds | Recipes | Decided by |
|---|---|---|---|
| `ports/core/` | Upstream software | 879 | It is somebody else's source |
| `src/packages/` | Our own software that is not the desktop | 11 | It is ours, and it is not a desktop component |
| `src/desktop/` | The desktop | 13 | It is ours, and it draws or serves the session |

Because all three use the same format, building the desktop is not a special case anywhere in the
build system. The phase environment names them in search order.

`src/libs/`, `src/build/` and `src/tools/` are not port repositories: the libraries are compiled by
their consumers' recipes, and the two tools are host-only and compiled on demand.

`src/packages/kdos-kpkg/` is the one directory here without a `kpkgbuild`. The package manager
cannot be built by the package manager, so `script/01_phase1/12_kpkg.sh` compiles it directly and
installs it as `kpkg` with four symlinks beside it.

## The library rule

Everything under `src/libs/` links nothing but the C library, with one declared exception:
`libkwl`, the Wayland backend. It is a separate archive precisely so the rule survives it, and it is
named only by a consumer that wants a display. `kinstall` links neither it nor a font renderer,
which is what lets the installer be built in phase 1.

Adding a dependency to any of the others moves every phase-1 consumer with it. See
[The C libraries](../05-developer/c-libraries.md).

## Generated and ignored

| Path | Gitignored | Notes |
|---|---|---|
| `build/` | entirely | The root filesystem, logs, snapshots, the ISO, signing keys, the bake's container store |
| `build-mobile/` | entirely | The mobile build root; firmware blobs land under it |
| `ports/core/*/​*.tar.*` | no | Upstream archives, tracked through Git LFS — see `.gitattributes` |
| `ports/.kpkg-meta`, `.kpkgbin/`, `.portup`, `.portup-tools/` | yes | Compiled host helpers |
| `ports/.update-cache.json` | yes | Per-port version-check results, 24-hour TTL |
| `docs/` except `docs/kdos/` and `docs/screenshots/` | yes | Scratch notes live there. A new documentation directory needs a line in `.gitignore` or it is silently uncommittable |
| `testing/logs/`, `testing/test_results.json` | yes | Runtime artefacts. `testing/fixtures/` is *not* ignored — those are the recorded corpora the self-test replays |

Clear the four `ports/` helpers when switching between a container run and a host run. A binary
built against one C library cannot execute under the other, and the failure does not say so. A
container run as root leaves them root-owned, so remove them from a container rather than reaching
for `sudo`.

## Files at the root

| File | Is |
|---|---|
| `README.md` | The front door |
| `CLAUDE.md` | Rules, conventions and workflow for working on this tree. Not a description of how the system works — that is this book |
| `Makefile` | Every target |
| `Dockerfile` | The build container |
| `.gitattributes` | Which paths go through Git LFS |
| `kdos.png`, `kdos.xcf` | The mascot |
| `plan.md` | The implementation plan for work in progress. A plan, not documentation |

The upstream tarballs go through Git LFS, and the filter must be installed before they are added: a
tarball staged before `git lfs install` is an ordinary blob until the history is rewritten.

The banner logo, the splash artwork and the icon marks are all generated from the mascot, so they
cannot drift apart from it.

## What must never exist

`fs/etc/X11/`. There is no X server on this system. The one carve-out is a rootless X server for
X11 clients inside boxes, run by the compositor, and it needs nothing in that directory. See
[Principles](../01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## See also

- [Architecture overview](../03-architecture/overview.md) — the three rings this tree implements
- [The build system](../05-developer/build-system.md) — how the phases traverse it
- [Writing ports](../05-developer/writing-ports.md) — the recipe format
- [Filesystem and IPC](filesystem-and-ipc.md) — the target's layout, not the source tree's
- [The programs](../04-programs/README.md) — what each source directory produces
