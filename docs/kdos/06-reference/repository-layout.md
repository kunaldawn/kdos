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
listed by shelf, with its phase, in [The ports catalogue](ports-catalogue.md). Terms such as *port*,
*box*, *pack*, *lane* and *rig* are defined in the [Glossary](glossary.md).

## The tree

The top-level directories divide by role. `src/` holds KDOS's own source: the C libraries, KDOS's
parts of the Plasma desktop, the daemons and the other programs written for KDOS. `ports/` holds the recipes for upstream software
and the tools that fetch its sources. `fs/` holds the files copied as they are into the target's
root filesystem. `script/` is the build, phase by phase; `testing/` is the tests and the test rig;
`build/` is where the build writes, and git ignores it. Five directories beneath `ports/` and
`src/` hold recipes; [The port repositories](#the-port-repositories) explains which, and how they
map onto the three rings of the running system.

```
kdos/
├── README.md              the front door
├── CLAUDE.md              the contributor briefing read by coding assistants
├── Makefile               every build and run target
├── Dockerfile             the build container
├── .dockerignore          what the build container's context leaves out
├── LICENSE
├── kdos.png, kdos.xcf     the mascot, from which the splash, logos and marks derive
├── .github/workflows/books.yml   renders both books, deploys them to GitHub Pages, and attaches
│                                 KDOS from Scratch to each release
│
├── docs/
│   ├── kdos/              this book
│   ├── kfs/               KDOS from Scratch, the build book: hand-written parts in
│   │   │                  numbered directories; package pages are generated, not kept
│   │   └── data/          the committed build-state snapshot `make book-data` writes
│   └── screenshots/       the pictures the documentation and the README show
│
├── ports/
│   ├── core/<shelf>/<name>/   one directory per upstream port, on one of 102 shelves
│   │   ├── kpkgbuild          declarative metadata — parsed, never sourced
│   │   ├── build.sh           the build; bash, run in the unpacked source
│   │   ├── postinstall.sh     optional install-time hook (27 ports have one)
│   │   ├── *.patch            optional patches, tracked (671 files)
│   │   ├── kfs.md             optional: the port's note in KDOS from Scratch; never hashed
│   │   ├── other support files  configuration, data files, single-file sources — tracked
│   │   └── <name>-<ver>.tar.* the upstream archive or vendor bundle — fetched, not tracked
│   ├── shelves                the closed list of shelves, one `<id> <volume> <description>` per line
│   ├── unbuilt                the recipes no phase builds on purpose, one name per line
│   ├── base                   the seeds of what the installation medium carries
│   ├── buildonly              ports the medium's closure does not follow a dependency into
│   ├── groups                 the native application groups `kdos update install` and the
│   │                          installer offer; staged into the image as /ports/groups
│   ├── Containerfile.fetch    the image that generates vendor bundles, pinning this tree's toolchains
│   ├── hackage-vendor         the Hackage downloader behind `vendoring = haskell`
│   ├── .srccache/             the local source cache, one file per hash — ignored
│   ├── sources.idx            which source-archive volume and asset hold each hash — committed
│   ├── .kpkgbin/, .portup, .portup-tools/   host helpers compiled on demand — ignored
│   ├── srclib.sh              the source archive's addressing, and the port lookup by name,
│   │                          shared by fetch, publish, the hook and preflight
│   ├── releaselib.sh          the release index's writer and shard rule, shared by publish,
│   │                          70_image/010_binhost.sh and the selftest
│   ├── fetch                  download and verify sources; generate vendor bundles (`make fetch`)
│   ├── publish                upload sources to the archive; freeze and publish a KDOS release
│   │                          with its binhost, slot image and release index
│   └── update                 front end of the upstream version checker (`make updates`)
│
├── src/                   KDOS's own code; every port is at exactly src/<area>/<name>/
│   ├── libs/              the C libraries, compiled by their consumers — see the rule below
│   │   ├── libkbase/          allocation, strings, files, processes, the trash
│   │   ├── libkbuild/         phases, plans, the snapshot inventory and its chains
│   │   ├── libkcolor/         the palette table and colour arithmetic
│   │   ├── libkpack/          the pack format
│   │   ├── libkpkg/           the package database, the ports tree and its lookup, the solver
│   │   ├── libkproc/          reads system state from /proc and /sys, or from a fixture root
│   │   ├── libksig/           Ed25519 — Monocypher, the one third-party source carried unmodified
│   │   ├── libktui/           the terminal toolkit: cell buffer, widgets, the installer's pages
│   │   ├── libkxdg/           desktop entries and the MIME cache
│   │   └── selftest.c         the shared assertion program
│   │
│   ├── system/            the system layer — a port repository, 8 recipes
│   │   ├── kdos-kpkg/         the package manager, under five names (no kpkgbuild)
│   │   ├── kinstall-core/     the installer's backend, compiled into kinstall (no kpkgbuild)
│   │   ├── kdos-installer/    kinstall, the terminal wizard over that backend; also built
│   │   │                      directly by 10_bootstrap
│   │   ├── kdos-appbox/       launching boxed applications, box management, the store lane
│   │   │   └── catalogue          every installable application, as a chain of apt packages
│   │   ├── kdos-boxinit/      process 1 inside a box; statically linked
│   │   ├── kdos-boxsock/      one tagged Wayland socket per box launcher
│   │   ├── kdos-doc/          this book on the image; `kdos` and `screenshots` link to docs/
│   │   ├── kdos-kfs/          KDOS from Scratch on the image; `docs` links to docs/kfs,
│   │   │                      `book` to script/book
│   │   ├── kdos-pack/         build, sign, index and diff packs
│   │   └── kdos-tools/        one binary: the kdos command, the ksvc supervisor, kdos-getty,
│   │                          kdos-bootctl and the other small system tools
│   │
│   ├── art/               pictures, themes and their generators — a port repository, 8 recipes
│   │   ├── kdos-theme/        the identity generators: colour schemes, Konsole, qt5ct,
│   │   │                      wallpapers, the greeter's palette
│   │   ├── kdos-icons/        the KDOS icon themes, the marks over Breeze; genmarks.py
│   │   ├── kdos-wallpapers/   the mark in each accent, as Plasma wallpaper packages
│   │   ├── kdos-sounds/       the sound theme, rendered by kdos-sfx at build time
│   │   ├── kdos-sddm-theme/   the login screen, the Breeze greeter with a KDOS patch
│   │   ├── kdos-addons/       the curated offline add-ons: wallpapers, schemes, scripts, widgets
│   │   ├── kdos-splash/       the boot splash, and the host-only boot-artwork generators
│   │   └── kdos-bb/           the ASCII-art demo, a fork of the AA-project's bb
│   │
│   ├── desktop/           KDOS's parts of the Plasma desktop — a port repository, 21 recipes
│   │   ├── kcm-kdos-*/        five System Settings pages: boxes, firewall, health, look, system
│   │   ├── kded-kdos/         the kded6 module that keeps the accent in step with Plasma
│   │   ├── kdos-plasma-look/  the global themes, the Plasma style, the desks, KSplash
│   │   ├── kdos-welcome/      the KDOS pages of the Welcome Center
│   │   ├── kdos-discover/     the Discover backend, the update notifier, the ports helper
│   │   ├── kdos-runner/       KDOS in KRunner, over its D-Bus runner API (C)
│   │   ├── kdos-notice/       the system's notices as Plasma notifications (C)
│   │   ├── kdos-kwin-motion/, kwin-effect-kdos-keys/, kwin-script-kdos-*/
│   │   │                      the KWin effects and scripts: motion, the key card,
│   │   │                      run-or-raise, window memory
│   │   ├── plasmoid-kdos-*/   the privacy lamps and the first-run checklist
│   │   ├── kio-library/       library:/, the offline library as a folder
│   │   ├── ksystemstats-kdos/ the KDOS sensors, face and page of System Monitor
│   │   ├── kdos-transcribe/   Transcribe in the file manager's menu
│   │   └── kdos-installer-gui/  the graphical installer over kinstall
│   │
│   ├── daemons/           root daemons the desktop account talks to — a port repository, 6 recipes
│   │   ├── kdos-powerd/       the firewall's services and the accent before login; client kdos-power
│   │   ├── kdos-energyd/      per-application energy attribution; client kdos-energy
│   │   ├── kdos-oomd/         memory-pressure protection
│   │   ├── kdos-packd/        the only thing that mounts a pack
│   │   ├── kdos-settingsd/    hostname1, timedate1 and locale1 over KDOS's files
│   │   └── kdos-sysupdated/   sysupdate1 and Slots1 over the A/B slots, for Discover
│   │
│   └── devtools/          host-side build tools — not ports, never installed on the target
│       ├── kdosbuild/         the build orchestrator
│       └── kdos-portup/       the upstream version checker
│
├── fs/                    copied verbatim into the target root filesystem
│   ├── etc/                   the system configuration; see the table below the tree
│   ├── root/                  root's home: .bash_profile and .bashrc
│   └── usr/                   local/bin and local/sbin: shell programs such as kdos-dm,
│                              kdos-session-wrapper and xdg-terminal-exec; local/lib/kdos: the
│                              session's shared shell code and the user timers; lib/turnstile:
│                              ksvc, the user-service backend script;
│                              share/{applications,bash-completion,desktop-directories,fwupd,
│                              glib-2.0,kdos}; KDOS's defaults for KDE programs are in
│                              share/kdos/xdg, boot artwork in share/kdos/boot; lib64 is a link
│                              to lib
│
├── script/                the build
│   ├── phases/<NN>_<name>/  the thirteen phases, run in number order; each holds its
│   │                        phase.env and either step scripts or a package list
│   ├── env/                 common.env, host.env, chroot.env — what the phase.env files
│   │                        share — and config.site, the autoconf site file chroot.env names
│   ├── bin/                 ninja and meson wrappers chroot.env puts first on PATH; never
│   │                        installed
│   ├── lib/port.sh          the recipe reader the two host phases source
│   ├── lib/imageset.py      the base image's closure over ports/base, which 017_stage.sh
│   │                        trims the image stage to and preflight checks; --why and
│   │                        the image ledger 088_ledger.sh writes
│   ├── chroot/              exec.sh runs a step in the chroot; enter.sh opens a shell there
│   ├── book/                the renderer of KDOS from Scratch (`make book`); kdos-kfs links it
│   ├── kdosbuild.sh         compiles and runs the orchestrator
│   ├── release-image.sh     a release's signed whole-slot image and its deltas
│   ├── migrate-phase-names.sh  renames the snapshots, logs and marks of a build/ tree
│   │                        made under other phase directory names to the current ones
│   └── hooks/pre-push       refuses a push that breaks the ports layout, adds a patch with
│                            no Subject:, fails the book checks or names an unarchived
│                            source (opt-in)
│
├── testing/
│   ├── preflight.sh          compiles kpkg, then checks the tree's wiring (packages resolve,
│   │                         recipes parse, scripts are valid) without building a port
│   ├── selftest.sh           the libraries and their consumers
│   ├── plasmatest.sh         the Qt suite of KDOS's desktop components (development image only)
│   ├── defaultscheck.py, xdgcheck.sh, lnfcheck.py, chordcheck.py, mimecheck.py, monofont.py
│   │                         KDOS's defaults for Plasma read back, kept out of /etc/xdg, the
│   │                         global themes and desks, the chords, the default applications and
│   │                         the monospace face
│   ├── docscheck.sh          both books: dead links, history, the page contract
│   ├── bookcounts.py         the book's port counts and catalogue rows, against the tree
│   ├── anchorcheck.py        every #fragment link in both books lands on a heading
│   ├── booknav.py            the chapter footers of both books, and KDOS from Scratch's contents
│   ├── kfscheck.py           KDOS from Scratch against the tree: notes, patch headers, kernel table, stamp
│   ├── phaseclosure.py       every package phase installs exactly the ports its list names
│   ├── recipemeta.py         every recipe's description, homepage and licence, and its catalogue row
│   ├── optdeps.py            features a recipe switches off although a port provides them
│   ├── visible-entries.allow the visible desktop entries preflight accepts though they read
│   │                         like helpers
│   ├── depdrift.py           link dependencies a package uses but its recipe does not declare
│   ├── debuginfo.py          fails a built tree that ships more than 1 MiB of debug information
│   ├── unowned.py, linkcheck.py, entrycheck.py
│   │                         the gates 70_image runs on the image: files nothing owns, run-time
│   │                         references nothing satisfies, links and launchers that lead nowhere
│   ├── treescan.py           the built-tree reader those four checks share
│   ├── hostcheck.sh          in a booted guest: is the binary each name resolves to the right one
│   ├── plasma-probe.sh, session-cost.sh, aboutcheck.sh
│   │                         in a booted guest: the Plasma session whole, its cost, the About rows
│   ├── atspi/                AT-SPI dumps of KDOS's surfaces and their goldens
│   ├── fixtures/             recorded system state, 30 directories
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
| Identity and login text | `os-release` (the version string; the image adds the build's commit and date), `issue`, `motd` |
| Accounts | `passwd`, `group`, `shadow`, `shells`, `subuid`, `subgid` |
| Network | `hostname`, `hosts`, `resolv.conf`, `nsswitch.conf`, `nftables.conf`, `nftables.d/` |
| Init and shells | `inittab`, `init.d/`, `service.disabled/` (one flag file per service that does not start; `sshd` ships there), `default/` (settings for `pcscd` and `useradd`), `fstab`, `mtab` (a link to `/proc/self/mounts`), `profile`, `profile.d/`, `bash.bashrc`, `skel/` |
| Kernel, modules and devices | `sysctl.conf`, `modprobe.d/`, `modules-load.d/`, `udev/`, `vtrgb`, `ld-musl-x86_64.path` |
| Login and sessions | `sddm.conf.d/`, `elogind/`, `turnstile/`, `security/limits.d/`, `locale.conf` |
| Desktop services | `polkit-1/`, `pipewire/`, `alsa/`, `samba/`, `fonts/conf.d/` (JetBrains Mono first for `monospace`), `xdg/` (the files every desktop reads; KDOS's defaults for KDE programs are in `fs/usr/share/kdos/xdg/`) |
| KDOS's own | `kdos/`: `packd.conf`, `menu.conf`, `zram.conf`, `xkb.conf`, `timers.d/`, `user-services/`, `keys/` |

The keys these files hold are listed in [Configuration](configuration.md).

## The port repositories

A *port* is one recipe: a `kpkgbuild` metadata file and a `build.sh` beside it (see
[Writing ports](../05-developer/writing-ports.md)). Five directories hold ports, and all five use
the same recipe format.

The port repositories, with `src/libs/`, carry the first two of the three *rings* described in
[Architecture overview](../03-architecture/overview.md#the-three-rings): `ports/core/` is the core
ring, and the four `src/` areas that hold ports, together with the libraries in `src/libs/` that
they compile in, are the desktop ring. The outer ring, the applications that run in boxes, is not a
port repository; it is the catalogue file `src/system/kdos-appbox/catalogue`.

| Directory | Holds | Recipes | Layout |
|---|---|---|---|
| `ports/core/` | Upstream software: somebody else's source, Plasma, Frameworks and Gear among it | about 2,400 | `<shelf>/<name>/`, on 102 shelves |
| `src/system/` | The package manager, the `kdos` command and its services, packs, boxes and the box socket, the installer, and the two books on the image (`kdos-doc`, whose `kdos` and `screenshots` link to `docs/kdos` and `docs/screenshots`, and `kdos-kfs`, whose `docs` and `book` link to `docs/kfs` and `script/book`) | 8 | `<name>/` |
| `src/art/` | The identity generators and what they make: icons, wallpapers, sounds, the login theme, the add-ons; the boot splash; the demo | 8 | `<name>/` |
| `src/desktop/` | KDOS's parts of the Plasma desktop, in Qt and QML (and two in C): settings pages, widgets, KWin effects and scripts, the Discover backend, the runner, the graphical installer | 21 | `<name>/` |
| `src/daemons/` | Root daemons whose client is the desktop account | 6 | `<name>/` |

The counts are directories holding a `kpkgbuild`; [The ports catalogue](ports-catalogue.md#ports-per-shelf) has the exact figures, shelf by shelf. A port is its bare name in every one of them:
one name is one port across all five, and nothing that names a port spells the shelf or the area.
Because the format is shared, building the desktop is not a special case anywhere in the build
system. The package manager finds a recipe through `PORT_REPO`, an ordered search path of at most
eight repositories; it looks for `<repo>/<name>/` and then `<repo>/<shelf>/<name>/`, so shelves
are never on the path themselves. Its default in `kpkg.conf` names the recipe tree a shipped image
carries, `/ports/core` and the four port-holding areas under `/ports/src`; inside the build's
chroot only `/ports/core` of those exists, and the phase environments widen it as the phases need
more:

| Phases | `PORT_REPO` |
|---|---|
| `20_selfhost`, `30_foundation`, `31_compilers`, `70_image` | not set: the default, of which only `/ports/core` exists |
| `40_lang` to `44_apps`, `60_kernel` | `/ports/core /kdos/src/system /kdos/src/art` |
| `50_desktop` | `/ports/core /kdos/src/system /kdos/src/art /kdos/src/desktop /kdos/src/daemons` |

The paths are the chroot's view of the tree, described in
[How the build sees the tree](#how-the-build-sees-the-tree). A port in `src/desktop/` or
`src/daemons/` is on the search path of `50_desktop` alone, so only that phase can build one: the
desktop's components link KWin, libplasma and Discover, which that phase installs.

Which port is built in which phase is decided by the lists in the phase directories, not by where
the recipe lives. Each package phase from `30_foundation` on names every port it installs, and
`testing/phaseclosure.py` fails the tree when a dependency pulls in a port its phase does not name.
[Which phase lists a port](../05-developer/writing-ports.md#which-phase-lists-a-port) gives the
rules, and [The ports catalogue](ports-catalogue.md) the phase of every port.

### ports/core and its shelves

Every upstream port sits at exactly `ports/core/<shelf>/<name>/`. A shelf is a subject, such as
`toolchain`, `audio-codecs` or `games-board`, and it only files the port: moving a port to another
shelf is one `git mv` and changes no recipe hash, package or index line. The exception is a port
that `.gitignore` names by path for an odd source suffix (`digikam`, `fluidr3-gm-sf3`,
`meshtastic-firmware`, `rnode-firmware`): its `.gitignore` line spells the shelf and is edited to
the new one in the same change, and `testing/preflight.sh` fails until it is. `ports/shelves` is the
closed list, one line per shelf giving its id and what belongs on it, in the order the package
lists and the catalogue group by. [Choosing a shelf](../05-developer/writing-ports.md#choosing-a-shelf)
places a new port.

Five checks hold the layout, in preflight and again in the pre-push hook: every recipe under
`ports/core` is exactly one shelf down; every shelf is listed in `ports/shelves`; one name is one
port across `ports/core` and `src/`; no shelf is named `libs`, `core` or after a port; and a
shelf id is lowercase letters, digits and hyphens.
Preflight alone also requires every listed shelf to have a volume and a description and to hold a
port, `name =` to equal the directory's
name, every `group =` family to sit on one shelf, and each file of a `packages.d/` list to name
only ports filed on its own shelf or in its own `src/` area.

### The src areas

`src/` holds six areas, and a port in any of them sits at exactly `src/<area>/<name>/`: its
`build.sh` reaches the libraries as `$PORT_SRC/../../libs`, which resolves at no other depth, and
`kdos-installer` compiles `../kdos-appbox/catalogue.c` from its sibling. A new program goes in the
first area that fits:

1. A library goes to `src/libs/`.
2. A program not installed on the target goes to `src/devtools/`.
3. A part of the Plasma desktop, a plugin, page, widget, effect or script Plasma loads or a
   program that speaks the session's D-Bus for it, goes to `src/desktop/`.
4. A root daemon whose client is the desktop account goes to `src/daemons/`.
5. Pictures, themes and their generators go to `src/art/`.
6. Everything else goes to `src/system/`.

`src/libs/` and `src/devtools/` are not port repositories. The libraries are compiled into each
program by that program's own recipe. The two tools run only on the build host and are compiled on
demand: `script/kdosbuild.sh` builds the orchestrator into `build/.kdosbuild`, recording what built
it in `build/.kdosbuild.sum` so an unchanged one is not recompiled, and `ports/update`
builds the version checker into `ports/.portup`. Preflight fails any other directory under `src/`
and any recipe at another depth, and fails the orphan sweep in
`script/phases/70_image/040_orphans.sh` when its list of repositories leaves out an area that holds
a recipe, since the sweep deletes from the image every package whose port it cannot find.

Two directories under `src/system/` are built by `10_bootstrap` scripts rather than through a phase
list, because that phase runs before the package manager exists:

- `src/system/kdos-kpkg/` has no `kpkgbuild`. The package manager cannot be built by the package
  manager, so `script/phases/10_bootstrap/120_kpkg.sh` compiles it directly and installs it as
  `/usr/bin/kpkg` with four symlinks beside it: `kpkgadd`, `kpkgbuild`, `kpkgdel` and
  `kpkgdepends`.
- `src/system/kdos-installer/` has a recipe, but no phase list names it.
  `script/phases/10_bootstrap/130_kinstall.sh` compiles `kinstall` from the same directory, taking
  every `.c` file by glob. The recipe's `build.sh` also compiles `src/system/kinstall-core`, the
  backend, which has no recipe of its own.

## The script directory

The orchestrator runs every directory under `script/phases/` whose name starts with digits and an
underscore, in sorted order. The numbers come in bands of ten with gaps, so a phase can be added
between two others without renaming either. A phase is named on the command line by its directory
name or by the part after the number (`--phases 41_system` or `--phases system`).

| Phase | Title | Runs | Holds |
|---|---|---|---|
| `00_cross` | Cross Toolchain | in the build container | step scripts, `00_binutils.sh` and `01_gcc.sh` |
| `10_bootstrap` | Base Userland | in the build container | step scripts, `000_file_system.sh` to `130_kinstall.sh` |
| `20_selfhost` | Self-Hosting Bootstrap | in the chroot | `packages.txt` |
| `30_foundation` | Build Foundation | in the chroot | `packages.txt` |
| `31_compilers` | Compilers | in the chroot | `packages.txt` |
| `40_lang` | Languages | in the chroot | `packages.d/` |
| `41_system` | System | in the chroot | `packages.d/` |
| `42_graphics` | Graphics Stack | in the chroot | `packages.d/` |
| `43_toolkits` | Toolkits | in the chroot | `packages.d/` |
| `44_apps` | Applications | in the chroot | `packages.d/` |
| `50_desktop` | Desktop | in the chroot | `packages.txt` |
| `60_kernel` | Kernel | in the chroot | `packages.txt` |
| `70_image` | Image | in the chroot; from `017_stage.sh` on, in the image stage over it | step scripts `010_binhost.sh` to `110_iso.sh`, `017_stage.sh` among them, which trims the stage to the base, and `088_ledger.sh`, which writes the image ledger, and `psf2limine.py`, which `110_iso.sh` runs |

A package phase keeps its list as one `packages.txt`, or as `packages.d/`, whose `*.txt` files are
read in byte order as one list: one file per shelf, `src-system.txt` and `src-art.txt` for ports of
KDOS's own, and `00-order.txt` for the runs a comment pins ahead of the rest. A phase has one or the
other, never both, and the orchestrator refuses to build when a phase has both, an empty
`packages.d/`, or neither a list nor a step.

Each phase's environment is the `phase.env` in its directory. The orchestrator reads that file's
own text for the phase's title, snapshot paths and `CHROOT=1`, and follows no `source` line, so
those keys are written in every `phase.env`. Everything shared is sourced from `script/env/`:

| File | Sourced by | Holds |
|---|---|---|
| `common.env` | every phase, through one of the two below | The reproducibility settings, the job count `KDOS_JOBS` with the `MAKEFLAGS`, `CMAKE_BUILD_PARALLEL_LEVEL` and `CARGO_BUILD_JOBS` it sets (under a jobserver, `GNUMAKEFLAGS` and `CARGO_MAKEFLAGS` in place of the last two), and `KPKG_STRICT_RECIPE=1` |
| `host.env` | `00_cross`, `10_bootstrap` | The target triplet, the sysroot and cross-toolchain paths, and the cross `pkg-config` setup |
| `chroot.env` | `20_selfhost` onwards | `PKG_CONFIG_PATH`, `CC` and `CXX`, the release flags, `CMAKE_BUILD_TYPE`, `CARGO_PROFILE_RELEASE_DEBUG`, `GOFLAGS` and the `CGO_*FLAGS`, the compiler cache, `script/bin/` first on `PATH` once ninja is installed, `CONFIG_SITE` naming `config.site`, `KPKG_SKIP_INDEX=man`; it removes nothing, since `kpkg` empties each port's own work directory |

`script/lib/port.sh` is sourced by the step scripts of `00_cross` and `10_bootstrap`, which run
before `kpkg` exists; it reads a recipe and unpacks its source, and finds a port by name one shelf
down, at `ports/core/<shelf>/<name>/`, failing when the name is on two shelves.
`script/lib/imageset.py` computes the [base image](../03-architecture/packaging.md#the-base-image)
from `ports/base` and `ports/buildonly` and checks those files and `ports/groups` against the tree;
`--why <port>` prints the dependency path from a seed to a port, and `--ledger` every installed
package of a root with its bytes and that path, which `70_image/088_ledger.sh` writes to
`build/image-ledger`; `--verify` fails a kept package whose recorded
[run-time needs](../03-architecture/packaging.md#run-time-needs) only a package outside the base
provides, which `70_image/087_gates.sh` runs on the stage.
`script/chroot/exec.sh` enters the chroot for a step or a package phase, roots the `70_image`
steps from `017_stage.sh` on in the image stage under `KDOS_IMAGE=base`, and
`script/chroot/enter.sh` opens an interactive shell in it. How a phase runs is in
[The build system](../05-developer/build-system.md).

## The library rule

No library is built as an archive of its own. Each program's recipe compiles the library sources it
needs straight into its binary.

The libraries a `10_bootstrap` program needs link nothing but the C library. That phase runs before
any other library exists on the target, and two programs are built in it:

| Program | Compiles in | Links |
|---|---|---|
| `kinstall`, the installer | `libkbase`, `libktui`, `libkcolor`, `kinstall-core` and `kdos-appbox`'s `catalogue.c` | the C library only |
| `kpkg`, the package manager | `libkbase`, `libkpkg`, `libksig` (with Monocypher) | the C library only |

If any of those five libraries gained a real link dependency, the program that compiles it in would
have to move to a later phase with it — and the package manager is what every later phase uses.

None of the nine libraries links anything beyond the C library. What a program links beyond them is
its own choice in its own `build.sh`: basu for the daemons on the system bus, libpng for
`kdos-theme`, Qt and KDE Frameworks for the components under `src/desktop/`, which compile in no
`libk*` library except `libkbase`, `libkxdg` and `libkcolor` where a part of them is C. The per-library
detail is in [The C libraries](../05-developer/c-libraries.md#the-set).

## How the build sees the tree

`make build` does not run the build in the checkout directly. It builds the `os-dev` image from the
`Dockerfile` (Alpine, with a host compiler and the usual build tools) and runs it with no network,
mounting only the directories the build reads:

| Checkout path | In the build container | Mode |
|---|---|---|
| `build/` | `/workspace/build` | read-write |
| `src/`, `fs/`, `script/`, `ports/`, `docs/`, `testing/` | `/workspace/src`, `/workspace/fs`, `/workspace/script`, `/workspace/ports`, `/workspace/docs`, `/workspace/testing` | read-only |
| `Makefile`, `Dockerfile` | `/workspace/Makefile`, `/workspace/Dockerfile` | read-only |

Of the root files only the `Makefile` and the `Dockerfile` are mounted, and the one thing either
changes in a build is the text of *KDOS from Scratch*, whose first part shows lines of both: an
edit to them stales the book's stamp, `docs/kfs/data/recipes.stamp`, and so rebuilds `kdos-kfs`
once the stamp is rewritten. `110_iso.sh` also copies both onto a medium made with
`KDOS_ISO_SOURCES=1`. `testing/` is
mounted for the tree gates `70_image` runs (`087_gates.sh`): they can stop a build, and nothing in
`testing/` is installed.
`docs/` is mounted for two ports, `kdos-doc` and `kdos-kfs`, which render the two books onto the
image; an edit to `docs/kdos` rebuilds the first and no other port, and an edit to `docs/kfs` the
second. The build writes only under `build/`; `build/fs/` is the target root
filesystem it grows phase by phase.

From `20_selfhost` on, each port is built inside a chroot of `build/fs/`. `script/chroot/exec.sh`
bind-mounts the container's `/workspace` at `/kdos` inside it, with `build/`, `script/`, `src/` and
`fs/` mounted again beneath it, `docs/`, `testing/`, the `Makefile` and the `Dockerfile` beneath it
read-only, and `ports/` at
`/ports` and again at `/kdos/ports`, where `kdos-kfs` and the image gates read the recipes. That is why the phase environments name
`/ports/core` and `/kdos/src/system`: they are the same directories as the checkout's
`ports/core/` and `src/system/`, seen from inside the chroot. The mechanism is in
[The build system](../05-developer/build-system.md).

A build run with `KDOS_ISO_SOURCES=1` has `script/phases/70_image/110_iso.sh` copy the tree onto the
ISO under `sources/`, beside the compressed system image rather than inside it: `src/`, `script/`,
`fs/`, the `Makefile` and the `Dockerfile` from `/kdos`, and every entry of `/ports` whose name does
not start with a dot, so each fetched source travels beside its recipe while the source cache and
the compiled host helpers stay behind. The step also names `CLAUDE.md`, which is not mounted into
the build and is skipped. A binary host the build wrote goes beside the tree as `sources/binhost`, and
`sources/SOURCES` records the number of recipes under `ports/core`, the size and the build time.

## Where the upstream sources are

Upstream source archives are not stored in git. A recipe names each of its files by its SHA-256
hash, and each archived file is a release asset of the `kunaldawn/kdos` repository, named
`<shelf>--<file>`, in the numbered pre-release `sources-<N>` that `ports/shelves` gives the shelf
whose port first names it; a file over GitHub's 2 GiB asset limit is stored in parts. `make fetch`
puts every file in its port directory, and a hash `ports/sources.idx` does not name comes from
upstream. `make build` runs its container with no network; only building that container's image
reaches out, for Alpine's packages. The lookup order, the cache, the vendor bundles and the
environment variables are described in [Developing](../05-developer/developing.md#where-sources-come-from).

Six files in the tree make this work:

| File | Does |
|---|---|
| `ports/srclib.sh` | The archive's addressing and hash checks, the lookup of a port by bare name on any shelf, and the on-demand build of the recipe reader; sourced by `ports/fetch`, `ports/publish`, the pre-push hook and `testing/preflight.sh` |
| `ports/sources.idx` | Format 2: the first line is `# kdos-sources-index 2`, then one line per archived file, `<sha256> <tag> <asset> <port>/<file>`, with `parts=<N>:<h1>,…,<hN>` after it for a file stored in parts. The file is asset `<asset>` (or `<asset>.part01` onwards) of release `<tag>`. Written by `ports/publish`, read by `ports/fetch` and the pre-push hook, checked by `testing/preflight.sh`, and committed with the recipe that needs it; an index of any other format is not read |
| `ports/fetch` | Resolves every recipe hash from the port directory, `ports/.srccache/`, the archive, or the recipe's `source =` URL, in that order, and generates a port's own vendor bundle when none of those holds it. `make fetch` runs it; `make fetch-check` runs `ports/fetch --check`, which is offline |
| `ports/publish` | Uploads sources the archive does not hold into their shelf's volume (needs a token) and writes their index lines; `--check` and `--dry-run` report without uploading; `--history` adds the files older commits' recipes name; `--plan` gives a new shelf its volume in `ports/shelves`; `--describe` rewrites each release's title and notes from the index; `--rehome` moves files whose port changed shelf, `--retire` deletes the emptied releases of the older one-per-shelf layout, `--orphans` lists files no recipe names; `--freeze <tag>` attaches a release's frozen `sources.sha256` list; `--release <tag>` puts up a KDOS release: the ISO, the recipe tree, checksums and their signatures, and its updates, the binhost in releases of at most 1,000 assets and the slot image, with the signed release index that finds them. See [Writing ports](../05-developer/writing-ports.md#publishing-sources) and [Publishing a release](../03-architecture/packaging.md#publishing-a-release) |
| `ports/releaselib.sh` | Writes the release index `kdos-update.idx`, makes a path's flat asset name and cuts a lane into releases of at most `$KDOS_RELEASE_CAP` assets; sourced by `ports/publish`, `70_image/010_binhost.sh` and `testing/selftest.sh` |
| `script/hooks/pre-push` | Refuses a `git push` that breaks the ports layout, adds or changes a patch with no `Subject:` line, fails the book checks, or whose recipes name a hash the archive does not hold, every part of a split file included |

The hook runs only in a clone that has opted in:

```sh
git config core.hooksPath script/hooks
```

With the hook on, a push passes four checks in order, and the first that fails stops it. The ports
layout and the patch headers are read from git's own tree, at the tip of each pushed ref the
remotes do not already hold, since only the tip is built; an intermediate commit that a later one
repairs passes. The layout check holds the five rules in
[ports/core and its shelves](#portscore-and-its-shelves); the patch check asks every new or changed
`.patch` or `.diff` under `ports/` or `src/` for a `Subject:` line. The book checks,
`testing/docscheck.sh` and `testing/kfscheck.py`, read the working copy rather than the pushed
commit. The archive check comes last, and refuses the push when
the archive cannot be reached, when it answers anything but 200 or 404, or when
`KDOS_SOURCES_BASE` is empty or a local directory, because the hook cannot then rule out that a
source is missing. Each refusal names the variable that skips its check: `KDOS_SKIP_LAYOUT_CHECK=1`,
`KDOS_SKIP_PATCH_CHECK=1`, `KDOS_SKIP_BOOK_CHECK=1` or `KDOS_SKIP_PUBLISH_CHECK=1`. Working offline
therefore means skipping the archive check for that push, which leaves the other three in force:

```sh
KDOS_SKIP_PUBLISH_CHECK=1 git push …
```

Setting `core.hooksPath` replaces `.git/hooks` as a whole, so any hook installed there, git-lfs's
included, stops running in that clone. Each check's exact rules are in
[The pre-push check](../05-developer/developing.md#the-pre-push-check).

## Generated and ignored

| Path | Ignored by git | Notes |
|---|---|---|
| `build/` | entirely | The root filesystem, logs, snapshots, the compiler cache `build/ccache`, the package store `build/pkgstore` (`<key[0:2]>/<key>/`, one package and its `META` each, written only with `KDOS_PKG_STORE` on), the ISO, signing keys, frozen source lists, and `build/podman/`, the pack bake's podman container store. After a build `script/kdosbuild.sh` hands everything back to your user except `build/fs/`, whose ownership is the shipped system's, the image stage's `build/image-upper/`, `build/image-work/` and `build/image-root/`, which hold the same files, and `build/podman/`, which podman requires to stay root's; reading either from the host needs a container or `sudo`. See [Where the build puts things](../05-developer/developing.md#where-the-build-puts-things) |
| `build_test/` | entirely | Where `testing/prepare_base.py` builds the minimal root filesystem that `testing/test_runner.py` builds ports against |
| `ports/core/*/*/*.tar`, `*.tar.*`, `*.tgz`, `*.tbz2`, `*.txz`, `*.zip`, `*.7z`, `*.part`, and a few more suffixes | yes | Upstream archives, vendor bundles and partial downloads, put there by `make fetch`, matched in a port directory one shelf down. A pattern for one port's odd suffix names its shelf, and must be edited when the port changes shelf. A patch or configuration file a recipe hashes is tracked and unaffected, and the archive fixtures under `testing/fixtures/` stay tracked |
| `ports/.srccache/` | yes | The source cache, `sha256-XX/<hash>`. Plain data: it survives `make clean` and `make cleanbuild` |
| `ports/.kpkgbin/`, `.portup`, `.portup-tools/` | yes | Compiled host helpers. `.kpkgbin/kpkg` is the recipe reader `ports/fetch` and `ports/publish` use, built by `ports/srclib.sh`; `.portup` is the version checker and `.portup-tools/` its own recipe reader. `.gitignore` also lists `ports/.kpkg-meta`, which nothing in the tree builds |
| `ports/.update-cache.json` | yes | Per-port version-check results, kept for 24 hours |
| `ports/sources.idx.lock`, `ports/sources.idx.tmp.*` | yes | `ports/publish`'s lock on the index, and its scratch copy while it rewrites it |
| `docs/` except `docs/kdos/`, `docs/kfs/` and `docs/screenshots/` | yes | Room for scratch notes. A new documentation directory needs a line in `.gitignore`, or git will not see anything in it |
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
| `Makefile` | Every target: building (`all`, the default and the same as `build`; `build`, `snapshots`, `cleanbuild`, `clean`), running (`run`, `run-hw`, `rundisk`, `rundisk-hw`, `debug-boot`, `check-hw`, `cleandisk`), sources (`fetch`, `fetch-check`, `updates`, `publish` and the seven `publish-*` targets), releases (`freeze`, `release`, `release-publish`), checks (`check`, and each of its checks alone, with `selftest-asan`, `plasmatest`, `booknav-write` and the built-tree checks `depdrift`, `debuginfo`, `unowned`, `linkcheck` and `entrycheck`), the book (`book`, `book-data`, `book-stamp`) and the rig (`quick`, `session-probe`, `shots`, `atspi`, `rig-image`, `devdeps-image`). `make help` lists them with a line each. `check-iso-free` runs before every `build` and stops it while another process — usually a running VM — holds `build/iso-build/kdos.iso` open, since rewriting the image under a guest gives it I/O errors; `ALLOW_ISO_IN_USE=1` overrides it. See [Developing](../05-developer/developing.md#make-targets) |
| `Dockerfile` | The build container, `os-dev`: Alpine with the host tools the two host phases (`00_cross`, `10_bootstrap`) need |
| `.dockerignore` | What the build container's context leaves out: `build/`, `ports-archived/` and `ports/` |
| `.gitignore` | The ignore rules in the table above |
| `.gitattributes` | Declares that no path goes through a filter |
| `.github/workflows/books.yml` | The one CI workflow. On a push to `main` it renders both books and deploys them as one GitHub Pages site, the KDOS book at the root and *KDOS from Scratch* under `kfs/` with `kfs/kfs.pdf`; on a published `v*` release it attaches `kfs.pdf` and `kfs-html.tar.zst`. Pages must deploy from GitHub Actions, since the tree holds no Jekyll configuration. See [Developing](../05-developer/developing.md#rebuilding-one-thing) |
| `LICENSE` | The licence |
| `kdos.png`, `kdos.xcf` | The mascot; `fs/usr/share/kdos/kdos.png` is a byte-identical copy shipped on the target |

Several pieces of artwork derive from the mascot. None of them is regenerated by the build, which
reads only committed files; each generator is a host-only Python script whose output is committed,
and it lives in a port directory rather than beside its output because `fs/` is copied verbatim
into the target, which carries no Python:

| Artwork | Committed file | Made from `kdos.png` by |
|---|---|---|
| Boot splash penguin | `src/art/kdos-splash/penguin.h` | Cropping and quantising the mascot; no script in the tree does it |
| Terminal banner logo | `fs/usr/share/kdos/logo.txt` | `src/art/kdos-splash/genlogo.py`, which decodes `penguin.h` |
| Icon marks | `src/art/kdos-icons/marks/` | `src/art/kdos-icons/genmarks.py` (needs Pillow) |
| The mascot in `kdos-bb` | `src/art/kdos-bb/src/kdostux.c` | `src/art/kdos-bb/genimg.py` (needs Pillow and a C compiler) |

Two more boot pictures contain the mascot without being generated from `kdos.png`.
`fs/usr/share/kdos/boot/kdos-banner.png` is committed as a picture; `genbanner.py` in
`src/art/kdos-splash/` only repaints its two caption lines. `kdos-backdrop.png` beside it, the
boot menu's backdrop, is drawn from the banner by `genbackdrop.py` in the same directory.

After changing `kdos.png`, regenerate the four in the table on the host (and copy the mascot to
`fs/usr/share/kdos/kdos.png`) and commit the results, or the artwork and the mascot drift apart.

## What must never exist

`fs/etc/X11/`. There is no Xorg server on this system and nothing X on the login path: SDDM's
greeter runs on Wayland. The one X server is Xwayland, a rootless X server KWin starts for X11
clients: boxed applications, and host applications that have no Wayland path. It needs nothing in
that directory. See
[Principles](../01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## See also

- [Architecture overview](../03-architecture/overview.md) — the three rings this tree implements
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — how these directories become an ISO
- [The build system](../05-developer/build-system.md) — how the phases traverse it
- [The ports catalogue](ports-catalogue.md) — every recipe under `ports/core/`, by shelf
- [How KDOS differs](../01-philosophy/how-kdos-differs.md) — why sources are pinned by hash and
  everything is built from this tree
- [Writing ports](../05-developer/writing-ports.md) — the recipe format
- [Filesystem and IPC](filesystem-and-ipc.md) — the target's layout, not the source tree's
- [The programs](../04-programs/README.md) — what each source directory produces

<!-- book-nav -->
---

*Part VI — Reference, chapter 42.* Previous: [41. Filesystem and IPC](filesystem-and-ipc.md) · [Contents](../README.md) · Next: [43. Known gaps](known-gaps.md)
