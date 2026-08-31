# Repository layout

The source tree, annotated. This is the map for finding where something lives; for the layout of a
**running** system, see [Filesystem and IPC](filesystem-and-ipc.md).

## The tree

```
kdos/
├── README.md              the front door
├── CLAUDE.md              rules, conventions and workflow for working on this tree
├── Makefile               every build and run target
├── Dockerfile             the build container
├── LICENSE
├── kdos.png, kdos.xcf     the mascot, from which the logo and marks are generated
├── *.plan.md              implementation plans for work in progress
├── docs.design.md         the design of record for this documentation
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
│   │   └── <name>-<ver>.tar.* upstream archive — a release asset, gitignored
│   ├── appbox/            the application catalogue
│   │   ├── packs.conf         the catalogue: bases, runtimes, applications, data
│   │   ├── bake               builds the pack set
│   │   ├── harvest.py         collects metadata from a built image
│   │   ├── Containerfile.bake the bake's own image
│   │   └── packs/             the baked set — gitignored
│   ├── Containerfile.fetch    the fetch image, pinning this tree's toolchains
│   ├── fetch                  download and vendor sources
│   ├── update                 the upstream version checker's front end
│   ├── sources                publish and fetch release assets
│   └── sources.manifest       what identifies each published archive
│
├── src/
│   ├── libs/              our C libraries — static, and see the rule below
│   │   ├── libkbase/          allocation, strings, files, processes, the trash
│   │   ├── libkcolor/         the palette table and colour maths
│   │   ├── libktui/           the terminal, the cell buffer, widgets, charts
│   │   ├── libkcell/          the glyph cache and the cell painter
│   │   ├── libkchrome/        the window furniture
│   │   ├── libkicon/          a name becomes a sprite slot, or −1
│   │   ├── libkwl/            the toolkit's Wayland backend
│   │   ├── libkxdg/           desktop entries and command-line splitting
│   │   ├── libkpkg/           the package database, the solver, the hashes
│   │   ├── libkpack/          the pack format
│   │   ├── libksig/           signing — the one vendored third-party source
│   │   ├── libkbuild/         phases, plans, the snapshot inventory
│   │   ├── libkproc/          every reading, from a movable root
│   │   └── selftest.c         the shared assertion program
│   │
│   ├── desktop/           the desktop — a port repository
│   │   ├── kdos-comp/         the compositor; KDOS additions in src/kdos-*.c
│   │   ├── kdos-shell/        one binary under 28 names
│   │   ├── kdos-res/          the resource monitor, and its setuid helper
│   │   ├── kdos-lock/         the lock screen, and the setuid password checker
│   │   ├── kdos-powerd/       suspend, poweroff, reboot
│   │   ├── kdos-energyd/      per-application energy attribution
│   │   ├── kdos-oomd/         memory-pressure protection
│   │   ├── kdos-mountd/       removable media
│   │   ├── kdos-packd/        the only thing that mounts a pack
│   │   ├── kdos-boxsock/      one tagged compositor socket per box
│   │   └── xdg-desktop-portal-kdos/  the file chooser, settings, app chooser
│   │
│   ├── packages/          ports that are OURS — a port repository
│   │   ├── kdos-kpkg/         the package manager, under five names
│   │   ├── kdos-installer/    the installer; links three libraries
│   │   ├── kdos-appbox/       launching boxed applications, and box management
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
│   ├── etc/{inittab,fstab,nftables.conf,profile.d,init.d,skel,kdos}
│   ├── usr/local/bin/         the session scripts
│   └── usr/share/{kdos,backgrounds}
│
├── script/                the build
│   ├── 00_toolchain/ … 06_packaging/    the phases
│   ├── *.env.sh                          per-phase environment and metadata
│   ├── kdosbuild.sh                      compiles and runs the orchestrator
│   └── chroot_exec.sh, chroot_enter.sh   the chroot
│
├── testing/
│   ├── preflight.sh          the wiring, in seconds
│   ├── selftest.sh           the libraries and their consumers
│   ├── fixtures/             recorded system state
│   ├── goldens/              committed reference frames
│   ├── vnc-shot.py           drive and photograph a real session
│   ├── packlane.sh           the application lane on a booted machine
│   ├── install-to-disk.sh    run the installer into a disk image
│   ├── appsweep.sh, appreport.sh   launch every application and report
│   ├── qemu-hw/              the containerised emulator
│   └── notes/                recorded measurements
│
└── build/                 generated — GITIGNORED IN ITS ENTIRETY
```

## What lives where, and why

Three directories hold ports, and they use **one recipe format**:

| Directory | Holds | Decided by |
|---|---|---|
| `ports/core/` | Upstream software | It is somebody else's source |
| `src/packages/` | Our own software that is not the desktop | It is ours, and it is not a desktop component |
| `src/desktop/` | The desktop | It is ours, and it draws or serves the session |

Because all three use the same format, **building the desktop is not a special case anywhere in
the build system**. The phase environment names them in search order.

`src/libs/`, `src/build/` and `src/tools/` are **not** port repositories: the libraries are
compiled by their consumers' recipes, and the two tools are host-only and compiled on demand.

## The library rule

Everything under `src/libs/` links **nothing but the C library**, with one declared exception — the
Wayland backend, which is a separate archive precisely so the rule survives it. Adding a dependency
to any of the others moves every phase-1 consumer with it. See
[The C libraries](../05-developer/c-libraries.md).

## Generated directories

| Path | Gitignored | Notes |
|---|---|---|
| `build/` | **entirely** | The root filesystem, logs, snapshots, the ISO, signing keys, the bake's container store |
| `ports/core/*/​*.tar.*` | yes | Upstream archives — release assets, fetched by bootstrap |
| `ports/appbox/packs/` | yes | The baked pack set |
| `ports/.kpkg-meta`, `.portup`, `.portup-tools`, `.kpkgbin` | yes | Compiled host helpers. **Clear them when switching between a container run and a host run** — a binary built against one C library cannot execute under the other |

## Files at the root

| File | Is |
|---|---|
| `README.md` | The front door |
| `CLAUDE.md` | Rules, conventions and workflow for working on this tree. **Not** a description of how the system works — that is this book |
| `Makefile` | Every target |
| `Dockerfile` | The build container |
| `kdos.png`, `kdos.xcf` | The mascot. The banner logo, the splash artwork and the icon marks are all generated from it, so they cannot drift apart |
| `*.plan.md` | Implementation plans for work in progress. Plans, not documentation |
| `docs.design.md` | The design of record for this documentation |

## What must never exist

**`fs/etc/X11/`.** There is no X server on this system, and there never will be. The one carve-out
is a rootless X server for X11 clients inside boxes, run by the compositor — and it needs nothing
in that directory. See [Principles](../01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## See also

- [Architecture overview](../03-architecture/overview.md) — the three rings this tree implements
- [The build system](../05-developer/build-system.md) — how the phases traverse it
- [Writing ports](../05-developer/writing-ports.md) — the recipe format
- [Filesystem and IPC](filesystem-and-ipc.md) — the target's layout, not the source tree's
- [The programs](../04-programs/README.md) — what each source directory produces
