<p align="center">
  <img src="kdos.png" alt="KDOS" width="260"/>
</p>

<h1 align="center">KDOS</h1>

<p align="center">
A Linux distribution compiled from source, one <code>kpkgbuild</code> at a time,<br>
with KDE Plasma as its desktop, made KDOS from the boot menu to the lock screen.
</p>

<p align="center">
<b>musl</b> · <b>GNU userland</b> · <b>sysvinit</b> · <b>KDE Plasma 6.8</b> · no systemd init · no Xorg · A/B updates
</p>

<p align="center">
<sub>about 2,400 upstream ports · 43 of its own · all of KDE Gear · Linux 7.2.11 · 73 containerised applications · builds with the network off</sub>
</p>

<p align="center">
  <img src="docs/screenshots/boot-splash.png" alt="The KDOS boot splash" width="100%"/>
</p>

<p align="center">
<sub>The boot splash, captured on a QEMU guest.</sub>
</p>

---

This page is for anyone meeting KDOS for the first time: what it is, how it differs from other
distributions, what it can do, how to try and build it, and where to read next. The full
documentation is a book under [`docs/kdos/`](docs/kdos/README.md).

## What KDOS is

KDOS is a complete operating system built in this repository from upstream source archives: a
cross toolchain, a musl userland, a self-hosting bootstrap, about 2,400 upstream ports with KDE
Plasma, KDE Frameworks and all of KDE Gear among them, KDOS's own settings pages, widgets, KWin
effects, Discover backend and look for that desktop, an installer, signed A/B system updates and a
package manager. There is no base image and no binary archive to fall back on.

Four properties shape the rest of the tree.

**It is built from source, and the exceptions are listed.** Firmware, microcode, radio images and
emulator ROMs ship as upstream built them, most of it because no source is published. The Rust,
Go, Zig, Haskell, Java and OCaml compilers each need a working compiler of their own kind, so each
starts from a pinned upstream bootstrap. The Noto and Nerd Fonts symbol fonts and the fonts some
applications bundle install upstream's built font files, some ports install data that has no
other source form (models, maps, game content), and a few web front ends ship the JavaScript
upstream built. The application catalogue is Debian binaries by
design, and runs only in boxes. The complete list is in
[Why KDOS](docs/kdos/01-philosophy/why-kdos.md#what-is-not-built-from-source).

**KDOS can build KDOS.** The build's self-hosting phase, `20_selfhost`, rebuilds the fortify
headers, tar, musl, zlib, binutils, diffutils, m4, gawk and gcc inside the new system, with
itself. The shipped system carries gcc, clang, binutils, rust, go, cmake, meson, ninja, python3,
make and the package manager, so a running installation can rebuild any single port, the compiler
and the kernel included, once its source is present. A full rebuild of the system needs a build
machine.

**The build runs offline.** Every source file a recipe uses is named by a `sha256 =` line in that
recipe. `make fetch` is the one step that downloads sources. After it, `make build` compiles in a
container started with `--network none`; only building that container's image needs the network.

**Applications are native ports, with boxes beside them.** Graphical applications are ported
natively, each with the toolkit it is written in: every KDE Gear application, web browsers, an
office suite, graphics and video editors, CAD, software radio and an offline reference library
among them. Their recipes sit
in `ports/core/` beside every other port, each on one of 102 subject shelves: GIMP is
`ports/core/graphics/gimp/`, Firefox ESR `ports/core/browsers/firefox-esr/`.
Beside them, a catalogue of 73 applications, each declared as a set of Debian packages over one of
seven shared runtimes or directly on the Debian base, is built by podman on the machine that
installs it, over the network. Each runs rootless in its own container, called a *box*.

## How it differs

Most distributions ship glibc, GNU coreutils, systemd, an Xorg server beside Wayland, and prebuilt
binary packages from an archive. KDOS keeps the GNU core userland and makes a different choice at
each of the other points: musl for the C library; sysvinit's `init` with runlevels, numbered shell
scripts and the `ksvc` supervisor in place of systemd, with elogind alone for logins and seats;
Wayland only, SDDM's greeter included, with rootless Xwayland for X11 clients; KDE Plasma built
unforked and made KDOS through settings, artwork and plugins rather than patches; and packages
compiled from about 2,400 recipes on a build host, with updates written into the machine's
inactive root slot and kept only once the new slot boots to a working desktop.
[How KDOS differs](docs/kdos/01-philosophy/how-kdos-differs.md) compares each choice with the
usual alternatives, says why KDOS chose differently, and lists what is given up.

## What it can do today

| Area | What is there |
|---|---|
| The desktop | KDE Plasma 6.8 on KWin, with three desks to choose from at first login, eight accents applied everywhere at once from the boot menu to the lock screen, an optional Phosphor mode with a CRT pass over the whole screen, KDOS pages in System Settings (boxes, the firewall, health, the look, the system's versions), a KRunner runner for the `kdos` verbs and an offline library, privacy lamps that name the box holding the camera, and Discover over ports, packs, boxes, firmware and system updates, which downloads nothing until you ask |
| Applications | Native ports compiled with the rest of the host, among them every KDE Gear application (Dolphin, Konsole, Kate, Okular, Kdenlive, digiKam, KMail and the rest), Firefox ESR, LibreWolf, Chromium, Thunderbird, LibreOffice, GIMP, Krita, Inkscape, Blender, FreeCAD, KiCad, OBS Studio, QGIS, Kodi, TeX Live and Wine, built Wayland-first with their X11 backends compiled in; and 73 catalogue applications, among them Zathura, Scribus, ParaView, Maxima, Scilab, VSCodium, Meld, Claws Mail and Hugin, each built on demand into its own box and [exportable as one file](docs/kdos/02-user-guide/applications.md#carrying-a-set-to-another-machine) that installs on another machine offline |
| The host | About 2,400 upstream ports built on musl: elogind, SDDM, PipeWire audio, NetworkManager, Xwayland for X11-only programs, Qt 5 and 6, QtWebEngine, KDE Frameworks 6, GTK 3 and 4, libadwaita and WebKitGTK, podman and QEMU, GCC and Clang/LLVM, Rust, Go, Zig, Haskell, Node.js and Python, cross toolchains for ARM, RISC-V and AVR, SDR and FPGA tooling, and CUPS printing |
| Boot and install | A boot splash that hands over to the login screen, an optionally encrypted root, A/B root slots filled by signed whole-slot images with deltas, and an installer with a graphical front end and a text-mode one over one backend, which also runs unattended from an answer file |
| Its own software | 9 C libraries written for this system, the package manager and the installer (both compiled in the bootstrap phase directly from `src/system/`), and 43 recipes of its own: 21 parts of the Plasma desktop, six root daemons, the box tools, the `kdos` command and its 42 subcommands, `help` included, and the artwork |

## Status and limits

This is the v0.2 line. Its interfaces are stable enough to document. It is not a release with a
support commitment or a tested hardware matrix: whoever runs it is the integrator.

The Plasma desktop, the KDE applications and KDOS's own parts of the desktop are recipes written,
wired into the build and tested in a development container; no image carrying this desktop has
been booted yet, and none of the natively ported applications has been started on a KDOS image.

Some things you may expect are not there. The login screen runs on a Wayland greeter its upstream
calls experimental; Phosphor mode needs hardware OpenGL; no release carries a system update or
prebuilt packages yet; the system speaks English only. Every such gap is listed in
[Known gaps](docs/kdos/06-reference/known-gaps.md), and the maturity of each subsystem, with the
evidence behind each verdict, is in [Status](docs/kdos/06-reference/status.md).

KDOS suits someone comfortable with a build log, a package recipe and the C behind its daemons.
It ships one user account, no telemetry, and no configuration layer between you and the file that
takes effect. Look elsewhere if you need broad hardware enablement, a large binary archive or
commercial support.

## Trying it

No v0.2 image is published. The [releases](https://github.com/kunaldawn/kdos/releases) on this
repository are `v0.1`, whose ISO belongs to an earlier line that the book does not describe, and
the numbered `sources-<N>` pre-releases, which hold the source archive and no image. To run v0.2, build it
(next section), then boot the result in a virtual machine:

```sh
make run          # QEMU with KVM and UEFI, 4 GB of memory, software rendering
make run-hw       # accelerated graphics, where Phosphor mode can run; needs Docker and the NVIDIA container toolkit
```

`make run` needs QEMU, `/dev/kvm` and the OVMF firmware at `/usr/share/ovmf/OVMF.fd`. The first run
creates a sparse 192 GB disk image, `build/kdos.qcow2`, for the installer to write to, large
enough for the two A/B system slots. The live image logs in to Plasma as `kdos` (password `kdos`)
by itself, and its Welcome Center offers the installer.

To try it on real hardware, write the image to a USB stick at least as large as the ISO
(`ls -l build/iso-build/kdos.iso`); every package is on the medium, so it does not fit a DVD, and
the installer offers only a disk at least as large as the installed system, whose size its first
page shows. This overwrites the whole of `/dev/sdX`:

```sh
sudo dd if=build/iso-build/kdos.iso of=/dev/sdX bs=4M status=progress conv=fsync
```

Boot the stick and install with *Install KDOS* on the desktop, or `sudo kinstall` in a terminal.
The installer writes nothing to disk until you confirm its summary page.

[Getting started](docs/kdos/02-user-guide/getting-started.md) walks through all of this, and
[Installation](docs/kdos/02-user-guide/installation.md) covers the installer page by page.

## Building from source

You need a Linux machine with Docker, `git`, `curl`, `sha256sum`, a C compiler, and Python 3 for
the pre-push hook's book checks. Every compiler
the build itself uses runs inside a container built from the repository's `Dockerfile`, so no
other toolchain is installed on your machine.

| Cost | |
|---|---|
| Time | Many hours for the first build: the whole system is compiled, GCC several times over and the kernel included. Later builds are narrow and short |
| Disk | About 42.2 GB (39.3 GiB) of upstream sources, plus tens of gigabytes under `build/`, and more for a complete set of the optional [phase snapshots](docs/kdos/05-developer/build-system.md#snapshots) (the tree compressed once plus a layer per phase), whose total has not been measured |
| Network | For `git clone`, `make fetch`, and the first `make build`, which builds its container image. The compile itself runs with the network off |

```sh
git clone https://github.com/kunaldawn/kdos
cd kdos
git config core.hooksPath script/hooks   # once per clone: the pre-push checks (see Contributing)
make fetch                               # every upstream source, verified by sha256
make fetch-check                         # optional, offline: anything missing or wrong
make build                               # the result is build/iso-build/kdos.iso
make run
```

A clone holds recipes, not upstream archives. `make fetch` takes each file from the local cache
`ports/.srccache/`, then from the KDOS source archive (release assets of `kunaldawn/kdos`, located
through the committed `ports/sources.idx`), then from the recipe's upstream URL, and keeps the
first copy whose hash matches. Run it again after a pull or a branch switch; it downloads only what
is new. On a terminal, `make build` first opens a
[picker](docs/kdos/05-developer/build-system.md#the-startup-picker) that asks whether to start
fresh or restore a snapshot, and whether to write snapshots during the run.

[How KDOS is built](docs/kdos/05-developer/how-kdos-is-built.md) follows the build from
`git clone` to a bootable ISO, phase by phase. [Developing](docs/kdos/05-developer/developing.md)
lists every make target and shows how to rebuild one port, one desktop component or only the
packaging instead of everything.

## The book

The documentation under [`docs/kdos/`](docs/kdos/README.md) is one book in six parts. Its index
page carries the full table of contents and four reading paths.

| Part | Covers | Start with |
|---|---|---|
| I. Introduction | What KDOS is, how it differs, its principles and decisions | [Why KDOS](docs/kdos/01-philosophy/why-kdos.md) |
| II. Using KDOS | Installing, the desktop, applications, theming, administration, accessibility | [Getting started](docs/kdos/02-user-guide/getting-started.md) |
| III. Architecture | Boot, the session, packaging, packs and boxes, security, design, windows | [Architecture overview](docs/kdos/03-architecture/overview.md) |
| IV. Programs | KWin, the Plasma shell, KDOS's settings pages, Discover, boxes, the daemons, the installer, the `kdos` command | [The programs](docs/kdos/04-programs/README.md) |
| V. Building and developing | The build, recipes, the C libraries, desktop software, testing | [How KDOS is built](docs/kdos/05-developer/how-kdos-is-built.md) |
| VI. Reference | Ports, commands, configuration, paths, gaps, status, glossary | [Command index](docs/kdos/06-reference/command-index.md) |

A second book, *KDOS from Scratch* under [`docs/kfs/`](docs/kfs/README.md), builds the system
phase by phase and package by package, with a page for every port. Both books are on every image,
under `/usr/share/doc/kdos` and `/usr/share/doc/kfs`, and both are published to the repository's
GitHub Pages site on every push to `main`; each `v` release carries *KDOS from Scratch* as one PDF
and as an HTML tarball.

## Repository layout

```
ports/core/       about 2,400 upstream ports on 102 shelves, ports/core/<shelf>/<name>/: a kpkgbuild
                  and a build.sh each, patches where needed
ports/            the shelf list (shelves), the fetch, update and publish tools, sources.idx,
                  and unbuilt (the recipes no phase builds)
src/libs/         9 C libraries written for this system
src/desktop/      KDOS's parts of the Plasma desktop: settings pages, widgets, KWin effects and
                  scripts, the Discover backend, the runner, the graphical installer
src/daemons/      the six root daemons the desktop talks to
src/system/       the package manager, the kdos command, packs, boxes and the box socket, the
                  installer, and the two books on the image
src/art/          the identity generators, icons, wallpapers, sounds, the login theme, the
                  add-ons, the boot splash, the demo
src/devtools/     kdosbuild (the orchestrator behind make build) and kdos-portup (behind
                  ports/update)
fs/               copied as-is into the target root filesystem
script/           kdosbuild.sh (what make build runs), the shared environments under env/,
                  the shared port lookup under lib/, the chroot's entry under chroot/,
                  the ninja and meson wrappers on the chroot's PATH under bin/,
                  and the pre-push hook under hooks/
script/phases/    the thirteen build phases, each with its phase.env and either its
                  numbered step scripts or its package list (packages.txt or packages.d/)
testing/          preflight, the self-test, the Qt suite, fixtures, the QEMU rig
docs/kdos/        the book
docs/kfs/         KDOS from Scratch, rendered by script/book/ (make book)
.github/          the workflow that publishes both books to GitHub Pages and each release
```

[Repository layout](docs/kdos/06-reference/repository-layout.md) annotates every directory.

## Contributing

Most contributions are ports: adding a piece of software or updating one. A port is a directory
on one shelf of `ports/core/`, `ports/core/<shelf>/<name>/`, holding two files, `kpkgbuild`
(declarative metadata) and `build.sh`, with any patches beside them and, for a port that needs
one, a [`kfs.md`](docs/kdos/05-developer/writing-ports.md#the-ports-note-kfsmd) explaining its
flags and patches for *KDOS from Scratch*.
[Writing ports](docs/kdos/05-developer/writing-ports.md) covers the format and walks through adding
a port end to end. A version bump looks like this:

```sh
ports/update <port>                 # accept the new version; the version and sha256 lines are rewritten and the source fetched
testing/preflight.sh                # the tree's wiring, in two to three minutes
grep -rlx <port> script/phases/*/packages*                              # which phase lists the port
make build BUILD_ARGS="--phases <phase>,70_image --rebuild <port>"     # that phase, e.g. 41_system
ports/publish <port>                # upload the new source and add its line to ports/sources.idx (maintainer token)
make book-stamp                     # rewrite docs/kfs/data/recipes.stamp for the new recipe and index
git commit ports/core/<shelf>/<port> ports/sources.idx docs/kfs/data/recipes.stamp
git push                            # the pre-push hook checks the layout, patches, the books and the archive
```

Uploading to the source archive needs write access to `kunaldawn/kdos`. Without it, run
`ports/publish --check <port>` to list what the archive lacks and name those ports in your pull
request, so a maintainer publishes them.

The pre-push hook runs four checks in turn, and each has a variable that skips it:

| Check | Refuses a push when | Skip with |
|---|---|---|
| Layout | at the tip of a pushed ref, a recipe is not exactly at `ports/core/<shelf>/<name>/`, its shelf is not listed in that commit's `ports/shelves`, a shelf is named `libs`, `core` or after a port, or a bare name is held twice across `ports/core` and `src/<area>/` | `KDOS_SKIP_LAYOUT_CHECK=1` |
| Patches | a new or changed `.patch` or `.diff` has no `Subject:` line saying what it changes | `KDOS_SKIP_PATCH_CHECK=1` |
| Books | `testing/docscheck.sh` or `testing/kfscheck.py` fails on the working copy, a stale recipe stamp included | `KDOS_SKIP_BOOK_CHECK=1` |
| Archive | a hash a pushed recipe names is missing from the committed `ports/sources.idx` or from the archive, or the archive cannot be reached at all | `KDOS_SKIP_PUBLISH_CHECK=1` |

The book check reads the working copy rather than the pushed commit, so push from a checkout of
what you are pushing. Testing describes what
[`docscheck.sh`](docs/kdos/05-developer/testing.md#docschecksh) and
[`kfscheck.py`](docs/kdos/05-developer/testing.md#kfscheckpy) measure.

Two rules apply to every change. A change updates the documentation that describes the behaviour
it changes, in the same commit. And the host carries no systemd init or service manager and no
Xorg server: elogind is the one login manager, and Xwayland the one X server. The
reasons are in [Principles](docs/kdos/01-philosophy/principles.md), and the test harnesses that check a change
before a full build are in [Testing](docs/kdos/05-developer/testing.md).

## Licence

The KDOS-authored parts are MIT (see [`LICENSE`](LICENSE)). `src/art/kdos-addons/LICENSE.notice`
records the palettes its colour schemes are made from and their licences. Monocypher, vendored in
`libksig`, is dual-licensed BSD-2-Clause or CC0-1.0. `kdos-bb` is a fork of a GPL-2.0 project and
keeps that licence along with its upstream copyright headers. `kdos-sddm-theme` is a patched copy
of Breeze's SDDM theme and keeps its LGPL-2.0-or-later licence. KWin's `kdos-crt.patch` adds files
under GPL-2.0-only. Every port under `ports/core/` is upstream's own code under upstream's own
terms.
