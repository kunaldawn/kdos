# How KDOS differs

This chapter compares KDOS, point by point, with the ways other Linux distributions solve the same
problems: which C library and userland to ship, how to start and supervise services, how to put
pixels on a screen, how to describe and build packages, how to deliver applications and updates,
and how much hardware to support. It is for a reader who knows Linux in general and wants to place
KDOS among the systems they already know before reading how it works. Read
[Why KDOS](why-kdos.md) first; it says what KDOS is for, and this chapter assumes it. Neither
chapter goes into mechanism. Each section ends by naming the chapter that does.

## The families KDOS is compared with

Linux distributions solve a common set of problems, and they fall into rough families by the
answer they give to the largest of them. This chapter uses six:

| Family | Examples | What defines it |
|---|---|---|
| Mainstream binary | Debian, Ubuntu, Fedora | Prebuilt packages from a large archive, broad hardware support, regular releases |
| Source-based | Gentoo, Linux From Scratch | The system is compiled on or for the machine; Linux From Scratch is a book you follow by hand |
| Minimal musl | Alpine, Void's musl flavour | A small C library and a compact userland, favoured for containers and small systems |
| Rolling | Arch | One continuously updated package set, no release boundary |
| Declarative and reproducible | NixOS, Guix System | The whole system is a function of a configuration; builds are isolated and results addressed by their inputs |
| Image-based | Fedora Silverblue and similar | The root filesystem is an image replaced as a whole; applications arrive separately, mostly as Flatpaks |

KDOS borrows from several of these and belongs to none of them. It is source-based like Gentoo,
uses musl like Alpine, keeps its packages reproducible in the way NixOS and Debian's
reproducible-builds work aim for, and separates applications from the base system as Silverblue
does. What it adds is a boundary between a small host that is compiled here and meant to be read,
and applications that are somebody else's packaging run in containers. [Why KDOS](why-kdos.md)
calls the two sides of that boundary the host and the boxes, and most of the differences below
follow from where that line is drawn.

## At a glance

| | Typical mainstream binary distribution | KDOS |
|---|---|---|
| C library | glibc | musl 1.2.6 |
| Core userland | GNU coreutils, util-linux | toybox 0.8.14, with util-linux, procps-ng and a few GNU tools where toybox falls short |
| Init | systemd | toybox `init`, numbered shell scripts, and `ksvc` as the supervisor |
| Display | Wayland and Xorg, a display manager | Wayland only, rootless Xwayland for X11 clients, no display manager |
| Host toolkits | GTK and Qt | None; every KDOS surface is a grid of character cells |
| Desktop | GNOME, KDE Plasma, Xfce and others | One desktop written for KDOS, on a frozen fork of the labwc compositor |
| Packages | Prebuilt binaries from an archive | Compiled from 1,038 recipes: on a build host for the installation image, on the machine itself for updates |
| Sources | Fetched from the archive's mirrors | Pinned by sha256, held in a content-addressed archive, built offline |
| Graphical applications | Distribution packages, Flatpak or Snap | Debian packages in rootless podman containers, built on demand |
| Updates | A package manager against a remote archive | A newer ports tree, compiled locally or matched against a binary host ([binhost](../06-reference/glossary.md)) you run yourself, optionally into a second root slot |
| Architectures | Several | x86-64 only |
| Support | Vendor or community, with a hardware matrix | None; whoever runs it is the integrator |

The rows are explained in the sections that follow. Each is a choice with a price, and the price is
stated with it.

## The C library

Debian, Ubuntu, Fedora, Arch, Gentoo in its default profile, NixOS and Guix all build on glibc,
the GNU C library. Alpine and the musl flavour of Void use musl, a smaller C library written for
standards conformance and static linking. KDOS is in the second group: its host is compiled against
musl 1.2.6, and the build targets the triple `x86_64-kdos-linux-musl`.

Alpine states its aims as a small, simple and secure system. KDOS chooses musl because it is small
enough to read, and the host is meant to be understood in full by one person.

The costs are the ones every musl distribution knows. Upstream code that assumes glibc extensions
needs a build flag or a patch, and a header warning that glibc does not emit can turn an upstream
`-Werror` build fatal. musl has no counterpart to glibc's `glibc-hwcaps`, the loader mechanism
that picks between several builds of a library for different CPU feature levels, so KDOS cannot
ship one binary tuned for several processor generations. Instead,
[`kdos march`](../04-programs/kdos-command.md#kdos-march) measures on the machine itself whether
building a port for the local instruction-set level makes it faster, which is possible because
every KDOS machine can rebuild its own packages; it records a verdict and installs nothing.

The one place glibc is present is inside the application containers, which are Debian. The host
and the boxes do not have to share a C library, and that is part of why the boundary exists. See
[Decisions](decisions.md#musl-as-the-host-c-library).

## The core userland

A mainstream distribution ships GNU coreutils, GNU grep, sed and findutils, and util-linux. Alpine
ships BusyBox, one multi-call binary providing compact versions of most of those commands. KDOS
takes the Alpine approach with a different binary: [toybox](https://landley.net/toybox/), another
multi-call userland, supplies `ls`, `cp`, `grep`, `init` and most everyday commands.

KDOS does not stop at toybox where toybox is not enough. It ships the full tool where the compact
one would lose a feature the system relies on, and the toybox build switches off almost every applet
whose name another port installs. The exceptions are `sed`, `find`, `xargs`, `awk`, `expr` and
`ln`, which the earliest build stage needs before the GNU ports exist, and which those ports take
over when they install. On the finished system each command has one owner:

- **util-linux** owns `mount`, `umount`, `swapon`, `blkid`, `switch_root` and several dozen
  others. toybox's `swapon` refuses `-a`, its `mount` runs no `mount.<type>` helper, and its
  `switch_root` would leave every process chrooted, which makes the kernel refuse the user
  namespaces rootless containers need.
- **procps-ng** owns `ps`, `top`, `free` and the rest of the process tools.
- **GNU sed, gawk, findutils and diffutils** are ports because upstream build systems reach for
  GNU extensions, and toybox ignores some of those extensions without reporting an error, so a
  build can go wrong without failing.
- **GNU coreutils** is built for two programs: `expr`, whose `length` operator toybox does not
  implement, and `ln`, whose `--relative` option toybox lacks and meson install scripts use.

The result is closer to Alpine than to Debian, with more full tools than Alpine's default image
carries. The cost is that a script written against GNU behaviour can meet a toybox applet that
behaves differently, and a new port occasionally has to find out which. The full list of what is
switched off, and why, is in
[Boot and init](../03-architecture/boot-and-init.md#tools-that-must-not-be-toyboxs).

## Init and service supervision

Most mainstream distributions, Arch and NixOS use systemd as init, service manager, logger, device
manager, login manager and more. Gentoo and Alpine default to OpenRC, Void uses runit, and Guix
uses the Shepherd. Linux From Scratch offers a System V init edition and a systemd edition.

KDOS uses no systemd component. Its PID 1 is toybox's `init`, configured by `/etc/inittab`, which
runs `/etc/init.d/rcS` at boot and `rcK` at shutdown. `rcS` runs the numbered scripts in
`/etc/init.d/` in order: 37 of them ship in the system files, from `01_udev.sh` to `82_ipp-usb.sh`,
and a few more are installed by their ports and do nothing until configured. A script is a service
table entry in shell. It starts its daemon under `ksvc`, a small supervisor written in C for KDOS,
which restarts the daemon when it exits and, when the service is stopped, stops everything the
daemon started. A service is disabled by creating a file named after it in
`/etc/service.disabled/`. Each of the other jobs systemd does, from device management to logging
and timers, is done by a separate program that does only that job;
[Principles](principles.md#no-systemd) lists them.

This is the same trade OpenRC, runit and s6 users make, and KDOS keeps it smaller still: there is
no dependency graph between services, only numeric order. The reason is legibility. A boot is a
list of shell scripts you can read top to bottom, and each daemon has one supervisor you can ask
about.

The price is paid where software expects systemd. There is no socket activation and no per-user
service manager. Rootless podman's resource limits depend on a cgroup subtree that systemd would
hand out, so KDOS has to provide that subtree itself and back it with a memory-pressure daemon.
lvm2's hotplug activation runs through `systemd-run`, so a volume group plugged in after boot has to
be activated by hand. There is also no single-user mode: toybox `init` ignores its arguments. See
[Principles](principles.md#no-systemd) and
[Boot and init](../03-architecture/boot-and-init.md#rcs-and-the-service-scripts).

## The display stack

Debian, Ubuntu and Fedora install a display manager (GDM, SDDM, LightDM) that draws a graphical
login and starts a session on Wayland or, where chosen, on an Xorg server. Arch and Gentoo leave
the choice to you, and Xorg is one of the options.

KDOS has no Xorg server port, no display manager and nothing X on the login path; the system files
contain no `/etc/X11/` directory. The graphical session is Wayland only. Xwayland is the single
exception: the compositor runs it rootlessly so that X11-only applications inside containers keep
working, and the X client libraries on the host exist only to build it. Mesa and Xwayland are
built without GLX, so an X11 client that draws through GLX gets no OpenGL; one that draws through
EGL is unaffected.

In place of a display manager, `tty1` logs in through `agetty`: automatically on the live image,
and with a password prompt on an installed system unless automatic login was chosen at install
time. That login shell's profile starts the desktop.

The reason is size and a single path. One display protocol means one way a window reaches the
screen, one input path and one security model for clients. The costs are that an application that
needs GLX does not get it, a workflow built on a remote X display has nowhere to run, and a
machine that wants a graphical greeter has none. See
[Principles](principles.md#no-xorg-server-and-one-carve-out).

## Toolkits on the host

On a mainstream desktop the base system carries GTK and usually Qt, because the desktop and its
applications are written in them. KDOS carries neither on the host.

Every surface KDOS draws, from the panel to the installer, is a grid of character cells drawn by
C libraries written for it; the boot splash, which runs before any of them, draws pixels straight
to the framebuffer. The compositor's window titlebars and menus are drawn with pango at a size
that lines up with the cell grid. Applications that need GTK or Qt run in containers, where both
toolkits are installed and themed through the shared home directory. The rule reaches libraries
too: a dependency that would pull a toolkit onto the host is built without it.

The reason is that the two toolkits together are larger than the rest of the host, and leaving
them out is what keeps the host small enough to compile in one sitting and to read. The cost is
that the host has no widget toolkit: anything a KDOS surface wants to show has to be expressible in
cells, and anything that is not goes in a box. See
[Principles](principles.md#no-gtk-and-no-qt-on-the-host).

## The desktop

Most distributions package one or more existing desktops and let you choose. KDOS ships one
desktop, written for this system, and no other.

It is a character-cell desktop in one of eight colour schemes, called accents, drawn through a
CRT-style phosphor shader that is on by default. The compositor, `kdos-comp`, is a frozen hard fork
of the labwc 0.20.0 Wayland compositor: upstream's source imported whole, never merged from again,
with KDOS's additions in their own files. Around it sit programs written for KDOS: the panel
`kdos-shell` with its menus and settings surfaces, an experimental terminal of its own, `kdos-term`,
beside `foot`, which is the default terminal, the resource monitor `kdos-res`, the lock screen
`kdos-lock`, and a set of daemons for power, mounting, memory pressure and application packs. They
share one palette of colour slots, one set of keys and pointer gestures, and one [design
language](../03-architecture/design-language.md).

The reason is that the cell grid is the project's identity, and a desktop from elsewhere would
bring the toolkits the previous section keeps out. KDE's applications are in the application
catalogue; Plasma is not on the host. The cost is that the desktop is one person's design, with
fewer features than GNOME or Plasma, and a fix upstream labwc makes has to be read and applied by
hand. See [Decisions](decisions.md#no-kde-gnome-or-any-existing-desktop-on-the-host) and
[Decisions](decisions.md#the-compositor-is-a-frozen-fork-of-labwc).

## Packages and recipes

Every distribution has a recipe format. Debian has source packages with a `debian/` directory and
`debian/rules`, Fedora has RPM spec files, Arch has `PKGBUILD` files, Gentoo has ebuilds, Void has
`xbps-src` templates, and NixOS and Guix have derivations written in the Nix language and in Guile
Scheme. Arch's `PKGBUILD` and Gentoo's ebuilds are bash scripts that the package tool sources, so
the metadata and the build are one program.

A KDOS port is two files. `kpkgbuild` holds the metadata as `key = value` lines, and it is parsed,
never sourced, so reading a recipe runs nothing. `build.sh` beside it is an ordinary bash script
run in the unpacked source. These are the metadata lines of `zlib`'s `kpkgbuild`, below its
banner comment:

```ini
name        = zlib
version     = 1.3.2
release     = 1
source      = https://zlib.net/$name-$version.tar.gz
sha256      = bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16  zlib-1.3.2.tar.gz
description = Compression library implementing the deflate compression method
```

The package manager, `kpkg`, is written for KDOS in C. It builds a port into a compressed tar
archive, records every path the package owns, removes files an upgrade drops, and resolves
dependencies from the `depends` lines. There are 1,014 recipes under `ports/core` for upstream
software and 24 under `src/` for KDOS's own, 1,038 in all, and all of them use the same format.
[The ports catalogue](../06-reference/ports-catalogue.md) lists every one of them by group.

The closest relatives are Arch's `PKGBUILD` and CRUX's `Pkgfile`, with the metadata pulled out so
that it can be parsed safely. Gentoo's USE flags, which let one ebuild build many configurations,
have no counterpart: each port builds one configuration, chosen in its `build.sh`. That is what
lets a prebuilt package be matched to a machine by exact equality rather than by comparing flag
sets. The cost is that changing how a package is configured means editing its recipe. See
[Packaging](../03-architecture/packaging.md) and
[Decisions](decisions.md#the-build-shell-lives-beside-the-recipe).

## How sources are pinned

Mainstream distributions keep their own copy of each upstream source inside their source packages,
and serve it from their archive's mirrors. Gentoo and Arch record checksums in the recipe and
download from upstream or a distribution mirror at build time. Nix and Guix fetch through
fixed-output derivations whose content hash is part of the recipe, with a binary cache in front.

KDOS names every upstream file by the `sha256 =` line in its recipe, and the hash is the file's
identity. `make fetch` is the only build step that uses the network. It takes each file from the
first place that holds a copy with the right hash: the port directory, a local cache, the KDOS
source archive, and finally the upstream URL. The source archive is a set of GitHub releases in
which each file is stored under its own hash and never replaced or removed, so a checkout from
years ago still finds the exact bytes it names after upstream has moved them. The recipes name
1,232 distinct files, about 8.3 GiB.

`make build` then runs its build container with `--network none`. A recipe that tries to download
anything during its build therefore fails on the machine that added it. Language ecosystems that
fetch their dependencies at build time (Rust, Go, Python, Haskell) get a vendor bundle, a
reproducible tarball of those dependencies that is itself hashed and archived; 124 ports carry one.

This is the Nix and Guix model of hash-pinned inputs applied to a conventional ports tree. The cost
falls on whoever adds a port: vendoring, publishing new sources to the archive before pushing, and
fixing every build system that wants the network. See
[Decisions](decisions.md#upstream-archives-are-content-addressed-release-assets) and
[Principles](principles.md#offline-by-construction).

## Self-hosting

A binary distribution is built on its own infrastructure, and an installed machine usually cannot
rebuild the system it runs without setting up that infrastructure first. Gentoo and Linux From
Scratch are self-hosting by nature: the compilers are on the machine and the system is rebuilt with
them. NixOS and Guix can rebuild any package locally and substitute a cached binary when one exists.
Guix goes further than any of these on the bootstrap itself, building its whole toolchain from a
very small binary seed.

KDOS is self-hosting in the Gentoo sense. The first build runs in numbered stages, called phases,
inside a container whose starting compilers come from Alpine 3.23 packages, the base of the build
image. A toolchain phase uses them to build a cross compiler, and phase 1 uses that to build a musl
userland with a native compiler. Phase 2 then rebuilds the compiler, the C library and the tools
they need inside a chroot, a directory tree the build enters as its root, using what phase 1 made,
so everything from phase 3 onwards is built by a compiler KDOS produced. The finished image keeps
gcc, clang, Rust, Go, the build tools and `kpkg`, so any single port, the kernel included, can be
rebuilt on the machine itself and offline; `kdos update apply` does that for every package that is
behind. [`kdos rebuild`](../04-programs/kdos-command.md#kdos-rebuild) is meant to drive the whole
build on a running machine, but it does not yet complete there.

KDOS does not attempt Guix's reduced-seed bootstrap. Four compilers written in themselves (Rust,
Go, Zig and GHC) start from upstream bootstrap binaries that are pinned by hash and never
installed, and the first compilers come from Alpine. See
[Why KDOS](why-kdos.md), [How KDOS is built](../05-developer/how-kdos-is-built.md) and the
[build system](../05-developer/build-system.md).

## Reproducibility

Debian and Arch run reproducible-builds testing across their archives, and a large share of their
packages build bit for bit identically; the rest are worked on package by package. NixOS and Guix
isolate every build and identify each result by a hash of its inputs, which makes rebuilding the
same thing reliable, and they check bit-for-bit reproducibility separately.

In KDOS a package built twice from the same tree is byte-identical, and that is a property of one
function rather than of each recipe. `kpkg` rolls every package archive itself, normalising what
would otherwise vary between two builds, and every build phase pins the environment a build can
observe. `kpkg verify --repro` builds a recipe twice and compares the two packages.

The reason is what reproducibility enables: a signed binary host whose packages can be checked
against a local build, and binary deltas whose output verifies against the original signature. The
constraint it imposes is that no recipe may roll its own archive. See
[Packaging](../03-architecture/packaging.md#reproducible-packages).

## Configuration

NixOS and Guix derive the system's configuration, `/etc` included, from one declaration, and a
change produces a new generation that can be rolled back. Other distributions configure through
files under `/etc`, often with tooling layered over them.

KDOS configures through the shipped files themselves: the files under `/etc`, and the KDOS-specific
ones under `/etc/kdos` such as `login.conf` and the timer definitions in `timers.d`. You edit them
in place, and there is no declaration they are generated from. The reason is the one
[Why KDOS](why-kdos.md#who-should-run-kdos) gives: there is no configuration layer between you and
the file that takes effect, so the file you read is the file that applies. The cost is that there
is no declarative rollback of configuration. The two root slots described under
[Updates](#updates) protect an update of the root, not a history of configuration edits. See
[Configuration](../06-reference/configuration.md).

## Applications

On Debian, Ubuntu, Fedora and Arch, a graphical application is usually a distribution package
installed into the base system, sharing its libraries. Flatpak and Snap deliver applications
separately from the base system: Flatpak as bundles over shared runtimes from a remote such as
Flathub, sandboxed with bubblewrap and reaching the desktop through portals (D-Bus services on
the host that open files, capture the screen and perform similar requests on an application's
behalf); Snap as compressed images from Canonical's store, confined with AppArmor. Silverblue
installs graphical applications as Flatpaks and uses Toolbx containers for command-line work.
Gentoo and Arch users build or install applications into the base system like everything else.

KDOS never installs a graphical application into the host. Its catalogue lists 180 applications
over 7 shared runtimes, each application declared as a chain of Debian trixie packages. Installing
one makes podman build a container image on the machine that asked, layer by layer over the shared
runtime, pinned to a snapshot of the Debian archive, and creates a rootless container, a *box*,
over the top image. The application then behaves like native software: it has a launcher entry, a
command on your `PATH`, file-type associations and the desktop's theme, and it reaches the host
through portals. A set of built applications can be exported as signed packs, read-only images
in EROFS, a compressed read-only filesystem format, that another machine imports without a
network; the pack daemon checks each pack's hash and signature when it installs it.

Compared with Flatpak, the model is similar in shape (shared runtimes, per-application artefacts,
portals) and different in three ways. The packages come from Debian's archive rather than from a
dedicated store, so the catalogue covers whatever Debian carries and adding an application is one
line in a text file. Nothing is downloaded prebuilt from a KDOS server: each machine builds its own
images. And the confinement is aimed at the desktop rather than at your files. A box shares your
home directory by default; what it cannot do, unless its box profile grants it, is bind the
compositor's screen-capture, input-method, clipboard-control or window-management protocols, so by
default it has to ask a portal on the host.

The costs follow from the same choices. A store install needs a network and minutes of apt, is built
from unsigned registry content, and is larger than the same program would be on Alpine. On a session
booted from the install medium, a store install works, because the container store runs through
fuse-overlayfs, but it is held in memory and lost at power-off unless the session has a persistence
store; an imported pack box's writable layer is always held in memory there. And the box is a
packaging and desktop boundary, not a jail for a malicious application. See [Packs and
boxes](../03-architecture/packs-and-boxes.md) and
[Decisions](decisions.md#a-store-that-builds-and-a-medium-that-carries-nothing).

## Updates

Debian and Fedora publish point updates and new releases from their archives, and `apt` or `dnf`
installs them into the running system. Arch rolls continuously. Gentoo syncs its tree and rebuilds
what changed. NixOS and Guix build a new system generation beside the old one and switch to it,
keeping earlier generations bootable. Silverblue downloads a new root image with rpm-ostree and
boots into it, keeping the previous one for rollback. ChromeOS and Android use two root partitions
and switch between them.

In KDOS a new version arrives with a newer ports tree, because each recipe pins its version. KDOS
is therefore neither rolling nor released in the Arch or Debian sense: a machine moves when you move
its tree. `kdos update check` compares what is installed with what the tree pins, and a system timer
runs it once a day. `kdos update apply` installs what is behind, compiling each package, or taking
it from a binary host when one is configured and holds an exact match. A binary host is a directory
of prebuilt packages with an index signed with Ed25519, which you run yourself on a disk, a stick or
a network mount; there is no KDOS archive server. A binary host can also carry binary deltas, which
are applied and then hashed against the signed index, so a delta never needs to be trusted.

On a machine set up with two root slots, `kdos update apply` installs into the inactive slot and
marks it to be tried on the next boot. The initramfs, the small early-boot filesystem that finds
and mounts the root, counts attempts, the end of a successful boot confirms the slot, and a boot
that never gets that far falls back to the slot known to work. This is the ChromeOS and Silverblue
idea of an update that cannot break the running system, applied to a package-built root.

Nothing installs updates automatically. A machine with no ports tree on it cannot update. The
installer lays out only the first slot, so creating and filling the second is a manual job, and
several of the slot paths have been exercised against fixtures rather than booted.
See [Packaging](../03-architecture/packaging.md#updating-a-machine),
[Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection) and
[Known gaps](../06-reference/known-gaps.md#boot-and-updates).

## The security model

Fedora enables SELinux in enforcing mode by default, and Ubuntu and Debian enable AppArmor. The
major binary distributions boot under UEFI Secure Boot through a signed shim. Their archives are
signed, and their security teams publish advisories and ship fixes.

KDOS is a single-user workstation, and its security model is narrower and explicit about it. It
defends against applications misbehaving, against tampered artefacts, against escalation through the
few privileged programs, and against catastrophic mistakes made through its own interfaces.
Graphical applications run in rootless containers whose compositor access is filtered to an
allowlist of Wayland protocols, widened for one box only by an explicit grant in its profile. The
root daemons obey callers by their credentials and never accept a path from them. Host sources are
verified by the hash in their recipe, prebuilt packages by the signed index, imported packs by a
hash and signature when they are installed, and packs on the install medium by the same check the
first time they are mounted. The shipped image carries nineteen setuid-root binaries, two of
them KDOS's own. `kdos cve` compares installed versions against a vendored copy of Alpine's security
database, offline.

It is equally explicit about what it does not do. There is no SELinux or AppArmor, no Secure Boot,
no verified or measured boot, and disk encryption is a passphrase typed at boot rather than a key
sealed in a TPM. Membership of `wheel` is effectively root. A box shares your home directory. There
is no security advisory service and no automatic update. The recipe tree is the trust root for
everything on the host. See [The security model](../03-architecture/security-model.md), and read
its last section first.

## Hardware and support

The mainstream distributions support several processor architectures, test on a wide range of
machines, and in some cases offer commercial support and certified hardware. Alpine, Void and
Arch have community support and broad hardware coverage from the same upstream kernel and firmware.

KDOS builds for x86-64 alone. Its kernel is built from source with one configuration, and the
complete `linux-firmware` tree ships unpruned, which covers a great deal of hardware, but nothing
is tested against a wide range of devices and broad hardware enablement is not a goal. The boot
loader is Limine, which is unsigned, so Secure Boot has to be turned off in the firmware. There is
no vendor, no support commitment, no migration guarantee between release lines and no tested
hardware matrix. See [Known gaps](../06-reference/known-gaps.md#hardware-and-platform) and
[Status](../06-reference/status.md).

## What you give up

Each section above states its own cost. Two are not stated elsewhere. The first build from a clean
checkout takes most of a day, and about 84 GB more disk if it keeps phase snapshots; see
[Developing](../05-developer/developing.md). Commercial software that assumes glibc and systemd on
the host does not run there; it runs in a box or not at all. Whether the trade as a whole is worth
it depends on what you want the machine for; [Why KDOS](why-kdos.md#who-should-run-kdos) says who
it suits.

## See also

- [Why KDOS](why-kdos.md) — what KDOS is and who it is for
- [Principles](principles.md) — the rules behind each difference above
- [Decisions](decisions.md) — the close choices and the alternatives that lost
- [Architecture overview](../03-architecture/overview.md) — the running system as a whole
- [Packaging](../03-architecture/packaging.md) — recipes, `kpkg`, the binhost and updates in depth
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the build from checkout to ISO, told end to end
- [The ports catalogue](../06-reference/ports-catalogue.md) — every port, by group
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — how applications are built and run
- [The security model](../03-architecture/security-model.md) — what is protected and what is not
- [Known gaps](../06-reference/known-gaps.md) — what does not exist

<!-- book-nav -->
---

*Part I — Introduction, chapter 2.* Previous: [1. Why KDOS](why-kdos.md) · [Contents](../README.md) · Next: [3. Principles](principles.md)
