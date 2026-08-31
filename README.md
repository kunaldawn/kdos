<p align="center">
  <img src="kdos.png" alt="KDOS" width="260"/>
</p>

<h1 align="center">KDOS</h1>

<p align="center"><b><i>I use KDOS btw.</i></b></p>

<p align="center">
A Linux distribution compiled from source, one <code>kpkgbuild</code> at a time —<br>
with a desktop that is a character grid all the way down.
</p>

<p align="center">
<b>musl</b> · <b>toybox</b> · <b>wlroots</b> · <b>no systemd</b> · <b>no Xorg</b> · <b>no GTK or Qt on the host</b>
</p>

<p align="center">
<sub>764 ports · Linux 7.0.10 · 758 packages · 181 containerised applications · builds offline from this repo</sub>
</p>

<p align="center">
  <img src="docs/screenshots/desktop.png" alt="The KDOS desktop: the panel along the bottom with live meters, desktop icons on the wallpaper, and a terminal" width="100%"/>
</p>

<p align="center">
<sub>Screenshots are 1920×1080, taken on the current image in a QEMU guest.
The phosphor shader is not in them — that renderer path declines on software
rendering, so what you see is the cell grid underneath it.</sub>
</p>

---

## What KDOS is

Four properties. Everything else in this repository follows from them, and each
is covered in [Why KDOS](docs/kdos/01-philosophy/why-kdos.md).

**Built from source, with named exceptions.** The host is compiled here from
upstream tarballs by 764 recipes: cross toolchain → musl userland →
self-hosting bootstrap → libraries → desktop → kernel. No base image, no binary
archive to fall back on. The handful of things that are *not* compiled here —
vendor firmware, two bootstrap compilers, two prebuilt font sets, the vendored
artwork and one cryptography implementation — are
[listed in full](docs/kdos/01-philosophy/why-kdos.md#what-is-not-built-from-source)
rather than glossed over.

**KDOS can build KDOS.** Phase 2 is a real self-hosting pass: inside the chroot
the system rebuilds tar, musl, zlib, binutils and gcc *with itself*. The shipped
system carries gcc, binutils, rust, cmake, meson, ninja, python3, make and the
package manager, so a running KDOS can rebuild every port in the tree — the
compiler, the kernel and the desktop included.

**The repository builds offline.** `make build` runs with `--network none`.
Every upstream tarball, every vendored bundle and every application image is
reachable at build time with no network at all. They are release assets rather
than repository contents; git holds what *identifies* them.

**Applications live in boxes.** KDOS builds the *desktop*. It does not
native-port Firefox, LibreOffice or Blender. The outer ring is a catalogue of
181 applications, each one signed image over a shared runtime, each running in
its own rootless container and behaving like ordinary system software.

---

## Who this is for

KDOS assumes a reader comfortable with a build log, a package recipe, and the C
that draws the panel. It ships one user account, no first-boot wizard, no
telemetry, and no configuration layer between you and the file that takes
effect.

It is worth your time if you want to **change** the desktop, the package manager
or the installer rather than configure around them; to run a workstation whose
applications are containerised by default; or to keep an offline, reproducible
source for the exact system you are running.

It is the wrong choice if you need broad hardware enablement, a large binary
archive, commercial support, or a system that stays out of your way. There is no
vendor to escalate to.

---

## Quick start

**There is no published ISO.** You build the image, which is the point rather
than a temporary state. Budget hours for the first build, tens of gigabytes of
disk, and a container runtime — nothing else is installed on your machine.

**Build an image**

```sh
git clone <this repository> kdos
cd kdos
make bootstrap        # fetch upstream sources — needs network, once
make build            # compile everything — no network at all
```

The result is `build/iso-build/kdos.iso`. The application catalogue is a
separate step: `make bootstrap-packs` downloads a baked set, or
`make fetch-packs` bakes one.
→ [Getting started](docs/kdos/02-user-guide/getting-started.md)

**Run it**

```sh
make run          # boot the ISO in a virtual machine
make run-hw       # the same, with accelerated graphics and the phosphor shader on
```

→ [Developing](docs/kdos/05-developer/developing.md)

**Install it**

```sh
sudo dd if=build/iso-build/kdos.iso of=/dev/sdX bs=4M status=progress conv=fsync
# boot it, then:
sudo kinstall
```

Nothing is written to your disk until the summary page.
→ [Installation](docs/kdos/02-user-guide/installation.md)

---

## What makes it different

### The desktop is a character grid

Every surface — the panel, the menus, the file dialog, the resource monitor, the
installer — is a grid of character cells drawn in one palette by our own
libraries. There is no GTK and no Qt on the host. The same code paints a
terminal, a Wayland window and an offscreen test frame, so a program looks
identical on `tty1` and under the compositor.

<img src="docs/screenshots/start-menu.png" width="100%"/>

→ [The design language](docs/kdos/03-architecture/design-language.md)

### The compositor renders through a phosphor pass

`kdos-comp` composites the desktop into a buffer of its own and blits it through
a GLES2 shader: scanlines, a horizontal bleed, a vignette, optional curvature.
The boot splash, the console and the session are finally the same machine. It is
on by default and `crt = 0` is an honest off.

→ [kdos-comp](docs/kdos/04-programs/kdos-comp.md)

### One application is one file

An application ships as a single signed filesystem image with its metadata,
icon, signature and a footer appended — mountable exactly as it sits. It runs in
its own container over a shared runtime. Installing one disturbs nothing else,
and an install carries only what was ticked.

→ [Packs and boxes](docs/kdos/03-architecture/packs-and-boxes.md)

### The monitor knows which application a process belongs to

Every fat application here is its own container, so a process table can name
them: `firefox-esr (appbox app.firefox-esr)` rather than forty rows of internal
process names. The same identity drives the per-application energy report and
the frame-drop attribution.

<img src="docs/screenshots/res-applications.png" width="100%"/>

→ [kdos-res](docs/kdos/04-programs/kdos-res.md)

### It can tell you why a frame was late

The compositor reports every late frame on a socket. `kdos stutter` joins that
to kernel pressure statistics and the process table and names who was busy —
and refuses to claim causation from a half-second sample, because that would be
wrong the first time two things were busy at once.

→ [The kdos command](docs/kdos/04-programs/kdos-command.md#stutter)

### Packages are reproducible, and that is load-bearing

A package built twice from the same tree is byte-identical. That is what makes a
signed index meaningful, what lets a delta reconstruct a package that still
verifies against the original signature, and what lets a rebuild be *compared*
to what it was built from.

→ [Packaging](docs/kdos/03-architecture/packaging.md)

### The medium is the software library

There is no application store. The Start menu lists what the medium already
carries, and choosing one installs the pack and opens the application in the
same action. A stick can also rebuild the image it booted from, and copy itself
to another stick.

→ [Applications](docs/kdos/02-user-guide/applications.md)

---

## Documentation

The full documentation is a book under [`docs/kdos/`](docs/kdos/README.md) —
forty pages in six parts, written for someone who intends to change this system
rather than configure around it.

| Start here | For |
|---|---|
| [Why KDOS](docs/kdos/01-philosophy/why-kdos.md) | What you are signing up for, and what is deliberately absent |
| [Getting started](docs/kdos/02-user-guide/getting-started.md) | Building an image, booting it, starting a session |
| [The desktop](docs/kdos/02-user-guide/desktop.md) | Panel, menus, windows, the full keybinding table |
| [Architecture overview](docs/kdos/03-architecture/overview.md) | The three rings, the host/box boundary, a process map |
| [The programs](docs/kdos/04-programs/README.md) | Every binary KDOS ships, and which page documents it |
| [Developing](docs/kdos/05-developer/developing.md) | The first build, and the loops that avoid one |
| [Command index](docs/kdos/06-reference/command-index.md) | Every command, alphabetically |
| [Known gaps](docs/kdos/06-reference/known-gaps.md) | What does not exist, so you stop looking |

Three reading paths — *use it*, *build it*, *change it* — are laid out on the
[book's index page](docs/kdos/README.md).

---

## Repository layout

```
ports/core/       764 upstream ports, two files each
ports/appbox/     the application catalogue
src/libs/         13 C libraries, linking nothing but musl
src/desktop/      the compositor, the shell, the daemons
src/packages/     our own ports: the package manager, the installer, the tools
fs/               copied verbatim into the target root filesystem
script/           the eight build phases
testing/          preflight, the self-test, fixtures, goldens, the QEMU rig
docs/kdos/        the documentation
```

Annotated in full in
[Repository layout](docs/kdos/06-reference/repository-layout.md).

There is no `fs/etc/X11/`, and there never will be.

---

## Status

This is the **v0.2** line: it installs, boots, runs a desktop and runs
applications, and its interfaces are stable enough to document. It is not a
release with a support commitment or a tested hardware matrix — you are the
integrator.

Maturity per subsystem, with the evidence behind each verdict, is in
[Status](docs/kdos/06-reference/status.md).

---

## License

MIT for the KDOS-authored parts. Vendored artwork keeps its upstream license —
see `LICENSE.notice` in `src/packages/kdos-cursors/`, `kdos-icons/` and
`kdos-gtk-theme/`, each of which records exactly what was changed. Monocypher is
public domain. `kdos-comp` and `kdos-bb` are forks of GPL-2.0 projects and keep
that license and their upstream copyright headers. Every port under
`ports/core/` is upstream's own code under upstream's own terms.

Go forth and segfault.
