<p align="center">
  <img src="kdos.png" alt="KDOS" width="260"/>
</p>

<h1 align="center">KDOS</h1>

<p align="center">
A Linux distribution compiled from source, one <code>kpkgbuild</code> at a time,<br>
with a desktop drawn entirely on a grid of character cells.
</p>

<p align="center">
<b>musl</b> · <b>toybox</b> · <b>wlroots</b> · no systemd · no Xorg · no GTK or Qt on the host
</p>

<p align="center">
<sub>853 upstream ports · Linux 7.0.10 · 833 packages in the image · 183 containerised applications · builds with the network off</sub>
</p>

<p align="center">
  <img src="docs/screenshots/desktop.png" alt="The KDOS desktop: the panel along the bottom with live meters, desktop icons on the wallpaper, and a terminal window" width="100%"/>
</p>

<p align="center">
<sub>1920×1080, captured on a QEMU guest. The phosphor shader declines on software
rendering, so the screenshot shows the cell grid underneath it.</sub>
</p>

---

## What KDOS is

KDOS is a complete operating system built in this repository from upstream
tarballs: a cross toolchain, a musl userland, a self-hosting bootstrap, 853
ports, a Wayland compositor, a panel, a terminal, an installer and a package
manager. There is no base image and no binary archive to fall back on.

Four properties shape everything else in the tree.

It is built from source, and the exceptions are listed rather than glossed over.
Vendor firmware and microcode ship prebuilt because no source exists. Rust and
Go each need a working compiler of their own kind, so both carry a pinned
upstream bootstrap. Four font sets ship as built faces, three sets of artwork are
vendored and recoloured at build time, and one cryptography implementation
(Monocypher) is third-party C compiled here like everything else. The complete
list, with versions and sizes, is in
[Why KDOS](docs/kdos/01-philosophy/why-kdos.md).

KDOS can build KDOS. Phase 2 is a genuine self-hosting pass: inside the chroot
the system rebuilds tar, musl, zlib, binutils, diffutils, m4, gawk and gcc using
itself. The shipped system carries gcc, binutils, rust, cmake, meson, ninja,
python3, make and the package manager, so a running installation can rebuild
every port in the tree — the compiler, the kernel and the desktop included.

The build runs offline. `make build` starts its container with `--network none`.
Every upstream tarball and every vendored bundle lives in the tree through Git
LFS, so a clone is the entire input to a build, and the `sha256 =` line in each
recipe sits beside the bytes it verifies.

Applications live in containers. KDOS builds the desktop, not Firefox. The outer
ring is a catalogue of 183 applications, each declared as a chain of Debian
packages over one of seven shared runtimes and built by podman on the machine
that asks for it. Each runs rootless, in its own container, and behaves like
ordinary system software.

## Who it is for

KDOS suits a reader comfortable with a build log, a package recipe and the C
that draws the panel. It ships one user account, no first-boot wizard, no
telemetry and no configuration layer between you and the file that takes effect.

Consider it if you want to change the desktop, the package manager or the
installer rather than configure around them; if you want a workstation whose
applications are containerised by default; or if you need an offline,
reproducible source for the exact system you are running.

Look elsewhere if you need broad hardware enablement, a large binary archive,
commercial support, or a system that stays out of your way. There is no vendor
to escalate to.

## Getting a system

No ISO is published. You build the image, which is the point of the project
rather than a temporary state. Budget several hours for the first build, tens of
gigabytes of disk, and a container runtime. Nothing else is installed on your
machine.

Install the Git LFS filter **before** cloning. Without it the archives arrive as
129-byte pointer files and the build fails on an unreadable archive rather than
on anything that names the cause.

```sh
git lfs install
git clone https://github.com/kunaldawn/kdos
cd kdos
make build
```

The result is `build/iso-build/kdos.iso`. It carries the catalogue rather than
the applications themselves; the machine that wants one builds it.

Boot the image in a virtual machine:

```sh
make run          # software rendering
make run-hw       # accelerated graphics, with the phosphor pass on
```

Write it to a stick and install:

```sh
sudo dd if=build/iso-build/kdos.iso of=/dev/sdX bs=4M status=progress conv=fsync
```

Boot the stick and run `sudo kinstall`. Nothing is written to disk until you
confirm the summary page.

Details are in [Getting started](docs/kdos/02-user-guide/getting-started.md),
[Installation](docs/kdos/02-user-guide/installation.md) and
[Developing](docs/kdos/05-developer/developing.md).

## What is unusual about it

### The desktop is a character grid

The panel, the menus, the file dialog, the resource monitor and the installer
are all grids of character cells, drawn in one palette by seventeen C libraries
written for this project. All but three of them link nothing beyond musl; the
exceptions take a font renderer, a pixel library and the Wayland client
libraries. No GTK and no Qt run on the host. The same code paints a terminal, a
Wayland window and an offscreen test frame, so a program looks identical on
`tty1` and under the compositor.

See [the design language](docs/kdos/03-architecture/design-language.md).

### The compositor renders through a phosphor pass

`kdos-comp` composites the desktop into a buffer of its own and blits it through
a GLES2 shader: scanlines, a horizontal bleed, a vignette, a phosphor floor and
optional tube curvature. The boot splash, the console and the session all pass
through the same machinery. The effect is on by default and `crt = 0` turns it
fully off.

See [kdos-comp](docs/kdos/04-programs/kdos-comp.md).

### An application is a row in a text file

Nothing is baked onto the medium. A catalogue row names the Debian packages an
application is made of and the runtime it sits on; the device builds a stack of
podman images, one per row, so a second GTK application costs one apt pass
rather than a whole image. Installing one disturbs nothing else.

Applications move between machines as files. `kdos app export` writes a built
application as a signed filesystem image with its metadata, icon and signature
in an appended footer, mountable as it sits, verified where it mounts, and
needing no network at the far end.

See [Packs and boxes](docs/kdos/03-architecture/packs-and-boxes.md).

### The resource monitor names applications, not processes

Because every heavyweight application is its own container, the process table
can attribute work to the thing a person recognises: `firefox-esr (appbox
app.firefox-esr)` rather than forty rows of internal process names. The same
identity drives the per-application energy report and frame-drop attribution.

See [kdos-res](docs/kdos/04-programs/kdos-res.md).

### It can tell you why a frame was late

The compositor publishes every late frame on a socket. `kdos stutter` joins that
stream to kernel pressure statistics and the process table and names what was
busy. It declines to claim causation from a half-second sample, which would be
wrong the first time two things were busy at once.

See [the kdos command](docs/kdos/04-programs/kdos-command.md).

### Reproducibility is load-bearing

A package built twice from the same tree is byte-identical. That property is
what makes a signed index meaningful, what lets a binary delta reconstruct a
package that still verifies against the original signature, and what lets a
rebuild be compared against what it was built from.

See [Packaging](docs/kdos/03-architecture/packaging.md).

### The software store is a file you can read

`kdos-store` presents seven curated groups over the 183 catalogue applications
and builds whatever you tick. There is no account and nothing to sign up to;
adding an application is one line in `src/packages/kdos-appbox/catalogue`. The
Start menu installs and opens in a single click. A booted stick can also rebuild
the image it came from and copy itself to another stick.

See [Applications](docs/kdos/02-user-guide/applications.md).

## Documentation

The full documentation is a book of 43 pages in six parts under
[`docs/kdos/`](docs/kdos/README.md), with reading paths for evaluating,
administering and developing KDOS.

| Start here | For |
|---|---|
| [Why KDOS](docs/kdos/01-philosophy/why-kdos.md) | What the project is for, and what is deliberately absent |
| [Getting started](docs/kdos/02-user-guide/getting-started.md) | Building an image, booting it, starting a session |
| [The desktop](docs/kdos/02-user-guide/desktop.md) | Panel, menus, windows, the full keybinding table |
| [Architecture overview](docs/kdos/03-architecture/overview.md) | The three rings, the host/container boundary, a process map |
| [Program map](docs/kdos/04-programs/README.md) | Every binary KDOS ships, and which page documents it |
| [Developing](docs/kdos/05-developer/developing.md) | The first build, and the loops that avoid one |
| [Command index](docs/kdos/06-reference/command-index.md) | Every command, alphabetically |
| [Known gaps](docs/kdos/06-reference/known-gaps.md) | What does not exist, so you stop looking |

## Repository layout

```
ports/core/       853 upstream ports, two files each
src/libs/         17 C libraries, most linking nothing beyond musl
src/desktop/      the compositor, the shell, the terminal, the root daemons
src/packages/     our own ports: package manager, installer, tools, theme
fs/               copied verbatim into the target root filesystem
script/           the eight build phases and their package lists
testing/          preflight, the self-test, fixtures, goldens, the QEMU rig
docs/kdos/        the documentation
```

Annotated in full in
[Repository layout](docs/kdos/06-reference/repository-layout.md).

There is no `fs/etc/X11/`, and there never will be.

## Status

This is the v0.2 line. It installs, boots, runs a desktop and runs applications,
and its interfaces are stable enough to document. It is not a release with a
support commitment or a tested hardware matrix; you are the integrator.

Maturity per subsystem, with the evidence behind each verdict, is in
[Status](docs/kdos/06-reference/status.md).

## License

The KDOS-authored parts are MIT. Vendored artwork keeps its upstream license:
see `LICENSE.notice` in `src/packages/kdos-cursors/`, `kdos-icons/` and
`kdos-gtk-theme/`, each of which records exactly what was changed. Monocypher is
public domain. `kdos-comp` and `kdos-bb` are forks of GPL-2.0 projects and keep
that license along with their upstream copyright headers. Every port under
`ports/core/` is upstream's own code under upstream's own terms.
