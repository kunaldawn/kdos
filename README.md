<p align="center">
  <img src="kdos.png" alt="KDOS" width="260"/>
</p>

<h1 align="center">KDOS</h1>

<p align="center">
A Linux distribution compiled from source, one <code>kpkgbuild</code> at a time,<br>
with a desktop whose every surface is a grid of character cells.
</p>

<p align="center">
<b>musl</b> · <b>toybox</b> · <b>wlroots</b> · no systemd · no Xorg · no GTK or Qt on the host
</p>

<p align="center">
<sub>1,014 upstream ports · 24 of its own · Linux 7.2.7 · 180 containerised applications · builds with the network off</sub>
</p>

<p align="center">
  <img src="docs/screenshots/desktop.png" alt="The KDOS desktop: the panel along the bottom with live meters, desktop icons on the wallpaper, and a terminal window" width="100%"/>
</p>

<p align="center">
<sub>1920×1080, captured on a QEMU guest. The phosphor shader turns itself off on software
rendering, so the screenshot shows the cell grid underneath it.</sub>
</p>

---

This README is the front door for anyone who has just found KDOS: what it is, what it can do, how
to try it, how to build it, and where to read next. The full documentation is a book under
[`docs/kdos/`](docs/kdos/README.md).

## What KDOS is

KDOS is a complete operating system built in this repository from upstream source archives: a
cross toolchain, a musl userland, a self-hosting bootstrap, 1,014 upstream ports, a Wayland
compositor, a panel, a terminal, an installer and a package manager. There is no base image and
no binary archive to fall back on.

It exists to answer one question honestly: what does a desktop look like when every part of it is
built here, from source you can read, and every exception is written down? Four properties follow
from that, and they shape everything else in the tree.

**It is built from source, and the exceptions are listed.** Firmware and microcode ship prebuilt
because no source exists for them. The Rust, Go, Zig and Haskell compilers each need a working
compiler of their own kind, so each starts from a pinned upstream bootstrap. A few ports carry
prebuilt data or firmware stubs inside their source, three sets of artwork (icons, cursors and a
GTK theme) are vendored and recoloured at build time, and the application catalogue is Debian
binaries by design. The complete list, with versions and sizes, is in
[Why KDOS](docs/kdos/01-philosophy/why-kdos.md#what-is-not-built-from-source).

**KDOS can build KDOS.** Phase 2 of the build is a self-hosting pass: inside the new system it
rebuilds tar, musl, zlib, binutils, diffutils, m4, gawk and gcc with itself. The shipped system
carries gcc, binutils, rust, cmake, meson, ninja, python3, make and the package manager, so a
running installation can rebuild every port in the tree, the compiler, the kernel and the desktop
included.

**The build runs offline.** Every source file a recipe uses is named by a `sha256 =` line in that
recipe. `make fetch` is the one step that touches the network: it gathers each file, checks it
against its hash, and keeps it. After that, `make build` runs in a container started with
`--network none`.

**Applications live in containers.** KDOS builds the desktop, not Firefox. Graphical applications
come from a catalogue of 180, each declared as a set of Debian packages over one of seven shared
runtimes and built by podman on the machine that asks for it. Each one runs rootless, in its own
container (a *box*), and behaves like ordinary system software.

## What it can do today

| Area | What is there |
|---|---|
| The desktop | A Wayland compositor (`kdos-comp`, a fork of labwc 0.20.0 with sixteen KDOS additions and a CRT-style phosphor shader), a panel program, `kdos-shell`, whose one binary provides 52 surfaces (the panel itself, the Start menu, launcher, settings, file chooser, notifications, network, Bluetooth, audio, displays, the store and more), a terminal, a resource monitor and a lock screen |
| Applications | 180 catalogue applications — among them Firefox ESR, Thunderbird, LibreOffice, GIMP, Krita, Inkscape, Blender, FreeCAD, KiCad, Kdenlive, OBS Studio, VSCodium and Wine — each built on demand into its own box, and movable between machines as a signed file |
| The host | 1,014 upstream ports built on musl: PipeWire audio, NetworkManager, Xwayland for X11 programs, podman and QEMU, GCC and Clang/LLVM, Rust, Go, Zig, Haskell, Node.js and Python, cross toolchains for ARM, RISC-V and AVR, SDR and FPGA tooling, and CUPS printing |
| Boot and install | A boot splash, an optionally encrypted root, A/B root slots, and `kinstall`, a text-mode installer that also runs unattended from an answer file |
| Its own software | 17 C libraries written for this system, and 24 recipes of its own: the desktop, five root daemons, the package manager, the installer, the `kdos` command and its 31 subcommands, `help` included |

<table>
<tr>
<td><img src="docs/screenshots/start-menu.png" alt="The Start menu"/></td>
<td><img src="docs/screenshots/res-applications.png" alt="The resource monitor's Applications page, with a process identified as belonging to a box"/></td>
</tr>
<tr>
<td><img src="docs/screenshots/settings.png" alt="kdos-settings, which opens on a grid of labelled pictures"/></td>
<td><img src="docs/screenshots/pick.png" alt="kdos-pick, the file dialog every boxed application reaches through the portal"/></td>
</tr>
</table>

## Status and honest limits

This is the v0.2 line. It installs, boots, runs a desktop and runs applications, and its interfaces
are stable enough to document. It is not a release with a support commitment or a tested hardware
matrix: whoever runs it is the integrator.

Some things you may expect are not there. Nothing reads the desktop aloud to a screen reader; there
is no graphical login screen; there is no public server of prebuilt packages; nothing creates the
second A/B root slot for you. Every such gap is listed in
[Known gaps](docs/kdos/06-reference/known-gaps.md), and how mature each subsystem is, with the
evidence behind each verdict, is in [Status](docs/kdos/06-reference/status.md).

KDOS suits someone comfortable with a build log, a package recipe and the C that draws the panel.
It ships one user account, no telemetry, and no configuration layer between you and the file that
takes effect. Look elsewhere if you need broad hardware enablement, a large binary archive or
commercial support.

## Trying it

No v0.2 image is published. The one release on this repository,
[v0.1](https://github.com/kunaldawn/kdos/releases), carries an ISO of an earlier line that the
book does not describe. To run v0.2, build it (next section), then boot the result in a virtual
machine:

```sh
make run          # QEMU with KVM and UEFI, 4 GB of memory, software rendering
make run-hw       # accelerated graphics with the phosphor pass on; needs Docker and the NVIDIA container toolkit
```

`make run` needs QEMU, `/dev/kvm` and the OVMF firmware at `/usr/share/ovmf/OVMF.fd`. Log in as
`kdos` with the password `kdos`; the session starts on the first virtual terminal.

To try it on real hardware, write the image to a USB stick, boot it, and install with
`sudo kinstall`. Nothing is written to disk until you confirm the installer's summary page:

```sh
sudo dd if=build/iso-build/kdos.iso of=/dev/sdX bs=4M status=progress conv=fsync
```

[Getting started](docs/kdos/02-user-guide/getting-started.md) walks through all of this, and
[Installation](docs/kdos/02-user-guide/installation.md) covers the installer page by page.

## Building from source

What you need: Docker, `git`, `curl`, `sha256sum` and a C compiler on a Linux machine. Every
compiler the build uses runs inside a container, so nothing else is installed on your machine.

What it costs:

| | |
|---|---|
| Time | Most of a day for the first build. The whole system is compiled, GCC several times over and the kernel included. Later builds are narrow and short |
| Disk | About 8.9 GB of upstream sources, plus on the order of 100 GB under `build/`, most of it optional phase snapshots (about 84 GB for a full set) |
| Network | For `git clone`, `make fetch`, and the first `make build`, which builds its container image. The compile itself runs with the network off |

The steps:

1. Clone the repository and turn on the pre-push check, once per clone:
   ```sh
   git clone https://github.com/kunaldawn/kdos
   cd kdos
   git config core.hooksPath script/hooks
   ```
2. Fetch the sources. A clone holds recipes, not upstream archives. `make fetch` takes each file
   from the local cache `ports/.srccache/`, then from the KDOS source archive
   (release assets of `kunaldawn/kdos`, each stored under its own sha256; the committed
   `ports/sources.idx` says which release holds which file), then from the
   recipe's upstream URL, and keeps the first copy whose hash matches. Rust, Go, Python and Haskell
   ports need a vendor bundle of their dependencies; one that no location holds is generated in a
   container.
   ```sh
   make fetch
   make fetch-check     # optional: confirms offline that every source is present and verified
   ```
3. Build. The result is `build/iso-build/kdos.iso`:
   ```sh
   make build
   ```
   On a terminal, the build first asks whether to start fresh or restore a snapshot.
4. Boot it with `make run`.

After a pull or a branch switch, run `make fetch` again; it downloads only what is new. To rebuild
one port, one desktop program or only the packaging instead of everything, see
[Developing](docs/kdos/05-developer/developing.md#rebuilding-one-thing).

## Where to read next

The book under [`docs/kdos/`](docs/kdos/README.md) has 43 pages in six parts, and three reading
paths through them:

| If you are… | Start with |
|---|---|
| Evaluating or installing KDOS | [Why KDOS](docs/kdos/01-philosophy/why-kdos.md), then [Getting started](docs/kdos/02-user-guide/getting-started.md) and [The desktop](docs/kdos/02-user-guide/desktop.md) |
| Running a KDOS machine | [Administration](docs/kdos/02-user-guide/administration.md), then [The kdos command](docs/kdos/04-programs/kdos-command.md) and [Configuration](docs/kdos/06-reference/configuration.md) |
| Working on the source | [Principles](docs/kdos/01-philosophy/principles.md), then [Architecture overview](docs/kdos/03-architecture/overview.md) and [Developing](docs/kdos/05-developer/developing.md) |

For a single command, [Command index](docs/kdos/06-reference/command-index.md) lists every one
KDOS installs; for the tree, [Repository layout](docs/kdos/06-reference/repository-layout.md)
annotates every directory.

```
ports/core/       1,014 upstream ports, two files each
src/libs/         17 C libraries written for this system
src/desktop/      the compositor, the panel, the terminal, the root daemons
src/packages/     the package manager, the installer, the tools, the theme
fs/               copied as-is into the target root filesystem
script/           the eight build phases and their package lists
testing/          preflight, the self-test, fixtures, goldens, the QEMU rig
docs/kdos/        the book
```

## Contributing

Most contributions are ports: adding a piece of software or updating one. A port is a directory
under `ports/core/` holding two files, `kpkgbuild` (declarative metadata) and `build.sh`.
[Writing ports](docs/kdos/05-developer/writing-ports.md) covers the format and walks through
adding a port end to end. A version bump looks like this:

```sh
ports/update <port>                 # accept the new version; the version and sha256 lines are rewritten and the source fetched
testing/preflight.sh                # the tree's wiring, in about two minutes
make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild <port>"
ports/publish <port>                # upload the new source and add its line to ports/sources.idx (maintainer token)
git commit ports/core/<port> ports/sources.idx
git push                            # the pre-push hook refuses a push naming a source the archive lacks
```

Uploading to the source archive needs write access to `kunaldawn/kdos`. Without it, run
`ports/publish --check <port>` to list what the archive lacks and name those ports in your pull
request, so a maintainer publishes them. The pre-push hook also refuses a push when it cannot reach
the archive at all; `KDOS_SKIP_PUBLISH_CHECK=1` skips the check for a push you know is safe.

Two rules bind every change. It updates the documentation that describes the behaviour it changes,
in the same change. And the host gets no systemd, no Xorg server, and no GTK or Qt; graphical
applications belong in a box. The reasons are in
[Principles](docs/kdos/01-philosophy/principles.md), and the test harnesses that check a change
before a full build are in [Testing](docs/kdos/05-developer/testing.md).

## Licence

The KDOS-authored parts are MIT (see [`LICENSE`](LICENSE)). Vendored artwork keeps its upstream
licence: see `LICENSE.notice` in `src/packages/kdos-cursors/`, `kdos-icons/` and
`kdos-gtk-theme/`, each of which records exactly what was changed. Monocypher is dual-licensed
BSD-2-Clause or CC0-1.0. `kdos-comp` and `kdos-bb` are forks of GPL-2.0 projects and keep that
licence along with their upstream copyright headers. Every port under `ports/core/` is upstream's
own code under upstream's own terms.
