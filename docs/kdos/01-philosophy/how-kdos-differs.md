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

KDOS borrows from several of these and belongs to none of them. It is source-based like Gentoo, uses
musl like Alpine, keeps its packages reproducible in the way NixOS and Debian's reproducible-builds
work aim for, and separates applications from the base system as Silverblue does. What it adds is a
boundary between a host that is compiled here from pinned source and meant to be read, and
applications that are somebody else's packaging run in containers. [Why KDOS](why-kdos.md) calls the
two sides of that boundary the host and the boxes, and most of the differences below follow from
where that line is drawn.

## At a glance

| | Typical mainstream binary distribution | KDOS |
|---|---|---|
| C library | glibc | musl 1.2.6 |
| Core userland | GNU coreutils, util-linux | GNU coreutils, grep, sed, gawk, findutils, diffutils, patch and tar, with util-linux and procps-ng; no multi-call userland |
| Init | systemd | sysvinit's `init` with runlevels, numbered shell scripts, and `ksvc` as the supervisor; elogind for seats and sessions |
| Display | Wayland and Xorg, a display manager | Wayland only, KWin; rootless Xwayland for X11 clients; SDDM with a Wayland greeter |
| Toolkits | GTK and Qt under the desktop and its applications | Qt 6 and KDE Frameworks under the desktop, and none under anything that runs as root or before the login; any toolkit for applications |
| Desktop | GNOME, KDE Plasma, Xfce and others | KDE Plasma and all of KDE Gear, in KDOS's own look, with an opt-in CRT mode |
| Packages | Prebuilt binaries from an archive | Compiled from about 2,400 recipes on the release's build host; the machine can rebuild any of them |
| Sources | Fetched from the archive's mirrors | Pinned by sha256, held in a release archive checked by hash, built offline |
| Graphical applications | Distribution packages, Flatpak or Snap | Native ports carried on the medium, plus Debian packages in rootless podman containers, built on demand; Discover shows both |
| Updates | A package manager against a remote archive | The release's signed slot image, or its signed packages, written into the inactive root slot and kept after a healthy boot; a source build on request |
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
takes the mainstream approach: every everyday command is the full GNU or util-linux program, each
a separate binary with one owner.

- **GNU coreutils** in full (`ls`, `cp`, `install`, `sort`, `stat` and the rest), with **GNU grep,
  sed, gawk, findutils, diffutils, patch and tar**. They are cross-built in the bootstrap phase and
  rebuilt as ports, so every configure script KDOS runs meets the tools upstream tested against.
- **util-linux** owns `mount`, `umount`, `swapon`, `blkid`, `switch_root`, `agetty`, `sulogin` and
  several dozen others.
- **procps-ng** owns `ps`, `top`, `free` and the rest of the process tools.
- **sysvinit** owns `init`, `telinit`, `shutdown`, `halt`, `reboot`, `poweroff`, `runlevel` and
  `killall5`, and nothing else.
- **musl's own** `getconf`, `getent` and `iconv`, and GNU `which`, fill the names glibc's tools
  hold elsewhere.

A compact userland differs from GNU in ways that fail silently: a mode an `install` did not apply,
an `awk` that printed nothing, a `grep` that matched nothing and reported no error. Every one of
those is a build that goes wrong without failing, and on a distribution that compiles every
package that cost lands on every port. The price of the full tools is size, a few megabytes more
than a multi-call binary, and a longer bootstrap phase. The alternatives that lost are in
[Decisions](decisions.md#gnu-at-every-name-no-multi-call-userland), which package owns each name is
in [Packaging](../03-architecture/packaging.md#the-base-userland), and the initramfs's own checks
are in
[Boot and init](../03-architecture/boot-and-init.md#tools-that-must-be-util-linuxs).

## Init and service supervision

Most mainstream distributions, Arch and NixOS use systemd as init, service manager, logger, device
manager, login manager and more. Gentoo and Alpine default to OpenRC, Void uses runit, and Guix
uses the Shepherd. Linux From Scratch offers a System V init edition and a systemd edition.

KDOS runs no systemd init, unit or service manager. Its PID 1 is sysvinit's `init`, configured by
`/etc/inittab`, which runs `/etc/init.d/rcS` at boot and `rcK` at shutdown. Runlevel 3 is the
running system, and runlevel `S` is single-user mode: `sulogin` asks for root's password on the
console, with no gettys and no desktop. `rcS` runs the numbered scripts in `/etc/init.d/` in
order: some ship in the system files, from `01_udev.sh` on, and the rest are installed by the ports
whose daemons they start. Most of the second kind, the servers among them, skip themselves until
their service is configured. A few start without configuration, among them the libvirt daemons,
and the Thunderbolt and dual-GPU daemons when that hardware is present. A script is a service
table entry in shell. One that runs a daemon starts it under `ksvc`, a small supervisor written in
C for KDOS, which restarts the daemon when it exits and, when the service is stopped, stops
everything the daemon started. A service is disabled by creating a file named after it in
`/etc/service.disabled/`, which is how `sshd` ships. Each of the other jobs systemd does, from
device management to logging and timers, is done by a separate program that does only that job;
[Principles](principles.md#no-systemd-init-units-or-service-manager) lists them.

Three of those programs come from systemd's own source tree and run without it: `eudev` for
device events, `basu` as the sd-bus library, and elogind, which tracks logins, seats and sessions
for SDDM, KWin, the lock screen and power management, as Alpine, Chimera Linux, Gentoo and Void
use it under Plasma. A user's services (the session bus, PipeWire, the user's timers) are started
at the first login and stopped at the last by turnstile, whose backend runs them under `ksvc`, so
they are supervised the same way the system's daemons are.

This is the same trade OpenRC, runit and s6 users make, and KDOS keeps it smaller still: there is
no dependency graph between services, only numeric order. The reason is legibility. A boot is a
list of shell scripts you can read top to bottom, and each daemon has one supervisor you can ask
about.

The price is paid where software expects systemd. There is no socket activation. Plasma's
packages install systemd user units that KDOS deletes, so Plasma takes its classic start-up path,
and KIO starts applications without the cgroup scopes System Monitor groups them by. DrKonqi, KDE's
crash dialog, needs `systemd-coredump` and is absent. lvm2's hotplug activation runs through
`systemd-run`, so a volume group plugged in after boot has to be activated by hand. See
[Principles](principles.md#no-systemd-init-units-or-service-manager),
[Decisions](decisions.md#sysvinit-as-pid-1) and
[Boot and init](../03-architecture/boot-and-init.md#rcs-and-the-service-scripts).

## The display stack

Debian, Ubuntu and Fedora install a display manager (GDM, SDDM, LightDM) that draws a graphical
login and starts a session on Wayland or, where chosen, on an Xorg server. Arch and Gentoo leave
the choice to you, and Xorg is one of the options.

KDOS has no Xorg server port and nothing X on the login path; the system files contain no
`/etc/X11/` directory, and the package manager refuses a port that installs one. The display
manager is SDDM, and its greeter runs on KWin as a Wayland compositor with no Xwayland, so no X
server exists before somebody logs in. The only session it offers is Plasma on Wayland: there is
no X11 session. Xwayland is the single exception: KWin starts it inside a session and runs it
rootlessly for X11-only applications, both those inside containers and host applications with no
Wayland path. The host carries the X client libraries those applications link, and Mesa and
Xwayland are both built with GLX, so an X11 client that asks for OpenGL through GLX finds it under
Xwayland.

The live image logs its `kdos` account in automatically. An installed machine shows the SDDM
greeter, unless automatic login was chosen at install time, which KDOS offers only on a machine
with one account and an encrypted root: the passphrase typed at boot has already said who is
there, and the session starts behind its lock screen unless the owner chose otherwise. The second
virtual terminal keeps a text login for recovery.

The reason is size and a single path. One display protocol means one way a window reaches the
screen, one input path and one security model for clients. The costs are that an X11-only
application runs through Xwayland, a workflow built on a remote X display has nowhere to run (the
desktop is shared over RDP or VNC instead), and the login screen runs on SDDM's Wayland greeter,
which its upstream still calls experimental. See
[Principles](principles.md#no-xorg-server-and-one-carve-out) and
[Decisions](decisions.md#sddm-with-a-wayland-greeter).

## Toolkits

On a mainstream desktop the base system carries GTK and usually Qt, because the desktop and its
applications are written in them. KDOS is the same where the desktop is concerned and stricter
everywhere else.

The desktop is Plasma, so Qt 6 and KDE Frameworks are under it, and KDOS's own desktop code (its
settings pages, plasmoids, KWin effects, Discover backend and the installer's graphical front end)
is Qt and KDE Frameworks too, and links no other toolkit. None of KDOS's own code that runs as
root or before the login links a toolkit at all: the package manager, the `kdos` command, the root
daemons, the installer's backend and its text front end, and the boot splash, which draws pixels
straight to the framebuffer before any of the rest exists. The login screen and the privileged
helpers of KDE's own settings pages are upstream's, and they use Qt.

Applications on the host are ported with their own toolkit (GTK 3 and 4, libadwaita, WebKitGTK,
Qt 5 and 6, QtWebEngine, KDE Frameworks, wxWidgets, FLTK or Tk). Every toolkit that has a Wayland
backend is built with it and with its X11 backend, and Wayland is the default at run time; Tk has
only an X11 backend, so a Tk application runs through Xwayland. GTK applications take the Breeze
GTK theme in the desktop's colours, and Qt 5 applications, for which no KDE Frameworks 5 exist
here, take the same colours through `qt5ct`. The containers carry the same toolkits for the
catalogue's applications. A library's toolkit front end is built where a ported application uses
it.

The reason for the line is that KDOS's code which keeps the machine running, updating and
recoverable must not depend on a graphical stack: a broken Qt takes the desktop down, and not the
means of repairing it. The cost is that the desktop's toolkits are larger than the rest of the host. See
[Principles](principles.md#the-desktop-is-kde-plasma).

## The desktop

Most distributions package one or more existing desktops and let you choose. KDOS ships one, KDE
Plasma, with all of KDE Gear beside it, and no other.

Most distributions that ship Plasma, Fedora's KDE edition and KDE's own KDE Linux among them, run
it on systemd, with Plasma's processes started as systemd user units and applications offered as
Flatpaks through Discover. Alpine and Chimera Linux are closer to KDOS: both build on musl and run
Plasma on elogind with no systemd, Alpine under OpenRC and Chimera under dinit with turnstile
starting each user's services, which is the arrangement KDOS has under sysvinit and `ksvc`. KDE
Linux is closer in another way: its operating system is an image, updated by writing a new one
beside the running one and asking for a restart, and KDOS updates the same way.

On top of Plasma KDOS adds its look and its integration. KDOS Modern is Breeze configured square
and painted in one of eight colour schemes, called accents, all generated from one palette, with a
Plasma style, a splash and a login screen in the same colours, so a machine wears one accent from
the boot menu to the desktop. Phosphor mode, off until somebody turns it on, adds a CRT pass over
the whole screen and CRT motion for windows. Plasma's own surfaces reach KDOS's mechanisms:
Discover installs native ports, packs and boxes and offers updates of the root slots, System
Settings has pages for boxes, the root slots, the firewall and the look, and KRunner answers KDOS's
commands. Windows float and snap to edges and quarters, and an automatic tiler is bundled, off.
KDOS's own [design language](../03-architecture/design-language.md) sets the rules all of that
follows.

The reason is that a desktop has to be good at a long list of things that are not specific to KDOS,
from screen readers to fractional scaling, and Plasma is. The cost is that KDOS's plugins that use
Plasma's internal interfaces are rebuilt and tested at every Plasma release, that Plasma and its
frameworks are more code than the rest of the host, and that the defaults are KDOS's: a person who
knows Plasma from elsewhere finds a different panel and colours until they change them. See
[Decisions](decisions.md#kde-plasma-and-all-of-gear-as-the-desktop) and
[Decisions](decisions.md#kdos-modern-with-phosphor-mode-on-request).

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
homepage    = https://zlib.net/
license     = Zlib
```

The package manager, `kpkg`, is written for KDOS in C. It builds a port into a compressed tar
archive, records every path the package owns, removes files an upgrade drops, and resolves
dependencies from the `depends` lines. There are about 2,400 recipes under `ports/core` for
upstream software and a few dozen under `src/` for KDOS's own, and all of them use the same
format.
The upstream recipes are filed on 102 subject shelves, `ports/core/<shelf>/<name>/`, much as T2
SDE files its packages by repository, while a port is still named by its bare name everywhere.
[The ports catalogue](../06-reference/ports-catalogue.md) lists every one of them by shelf, and
[Decisions](decisions.md#ports-are-shelved-by-subject-identity-is-the-bare-name) says why a shelf
is not part of a port's name.

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
2,572 distinct files, about 39.3 GiB.

`make build` then runs its build container with `--network none`. A recipe that tries to download
anything during its build therefore fails on the machine that added it. Language ecosystems that
fetch their dependencies at build time (Rust, Go, Python, Haskell) get a vendor bundle, a
reproducible tarball of those dependencies that is itself hashed and archived; 173 ports carry one.

This is the Nix and Guix model of hash-pinned inputs applied to a conventional ports tree. The cost
falls on whoever adds a port: vendoring, publishing new sources to the archive before pushing, and
fixing every build system that wants the network. See
[Decisions](decisions.md#upstream-archives-are-hash-checked-release-assets) and
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
image. The first phase, `00_cross`, uses them to build a cross compiler, and `10_bootstrap` uses
that to build a musl userland with a native compiler. `20_selfhost` then rebuilds the compiler, the
C library and the tools they need inside a chroot, a directory tree the build enters as its root,
using what `10_bootstrap` made, so everything from `30_foundation` onwards is built by a compiler
KDOS produced. The finished image keeps gcc, clang, Rust, Go, the build tools and `kpkg`, so any
single port, the kernel included, can be rebuilt on the machine itself, offline once its sources are
on disk; `kdos update apply --source` does that for every package that is behind, downloading the
sources first, in place of the release's signed binaries an update installs by default. [`kdos
rebuild`](../04-programs/kdos-command.md#kdos-rebuild) runs the whole build on a booted machine; no
run of it from the install medium has been taken through every phase (see [Known
gaps](../06-reference/known-gaps.md#an-offline-kdos-rebuild-from-the-medium-has-not-been-run-to-the-end)).

KDOS does not attempt Guix's reduced-seed bootstrap. Six compilers written in themselves (Rust,
Go, Zig, GHC, OpenJDK and OCaml) start from upstream bootstrap binaries or images that are pinned
by hash and never installed, and the first C compilers come from Alpine. See
[Why KDOS](why-kdos.md), [How KDOS is built](../05-developer/how-kdos-is-built.md) and the
[build system](../05-developer/build-system.md).

## Reproducibility

Debian and Arch run reproducible-builds testing across their archives, and a large share of their
packages build bit for bit identically; the rest are worked on package by package. NixOS and Guix
isolate every build and identify each result by a hash of its inputs, which makes rebuilding the
same thing reliable, and they check bit-for-bit reproducibility separately.

In KDOS a package built twice from the same tree is byte-identical, and that is a property of one
function rather than of each recipe. The one exception is the kernel package: each module is
signed with ECDSA, which draws a fresh random number for every signature, so the modules'
signatures differ between two builds. `kpkg` rolls every package archive itself, normalising what
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

KDOS configures through the shipped files themselves: the files under `/etc`, the KDOS-specific
ones under `/etc/kdos` such as the timer definitions in `timers.d`, and Plasma's own files. You edit
them in place, and there is no declaration they are generated from. Plasma reads each of its files
through the configuration search path: a user's copy under `~/.config` first, then the machine
owner's under `/etc/xdg`, then KDOS's defaults under `/usr/share/kdos/xdg`, so a distribution
default never overwrites a choice and System Settings writes the same files you would. The reason
is the one [Why KDOS](why-kdos.md#who-should-run-kdos) gives: every setting is a file you can
open, with no generator between you and it. The cost is that there is no declarative rollback of
configuration. The two root slots described under
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

KDOS ports graphical applications natively, so that a machine installed from its media has a
browser, an office suite, media and graphics tools, maps and an offline library with no network.
They are ordinary recipes in the ports tree, filed on shelves named for what they are for and built
mostly in the `43_toolkits` and `44_apps` phases, with those that need no toolkit, such as mpv, in
`42_graphics`; [The ports catalogue](../06-reference/ports-catalogue.md) lists them. Beside them,
the catalogue of boxed applications lists 70 applications, each declared as a few Debian trixie
packages over a shared Debian base and, for most of them, one of seven shared runtimes (GTK, Qt,
KDE, media, science, Electron and Wine). Installing one makes podman build a container image on the
machine that asked, layer by layer over the shared runtime, pinned to a snapshot of the Debian
archive, and creates a rootless container, a *box*, over the top image. The application then behaves
like native software: it has a launcher entry, a command on your `PATH`, file-type associations and
the desktop's theme, and it reaches the host through portals. A set of built applications can be
exported as packs, read-only images in EROFS, a compressed read-only filesystem format, that another
machine imports without a network. The pack daemon checks each pack's hash when it installs it, and
its signature when it carries one; an unsigned pack is accepted unless the daemon is told to require
signatures. Discover is the front door to all of it: it lists the native ports, the catalogue and
the installed boxes, and installs and removes them, beside updates of the system and firmware. There
is no Flathub, no Snap and no "Get New…".

Compared with Flatpak, the model is similar in shape (shared runtimes, per-application artefacts,
portals) and different in three ways. The packages come from Debian's archive rather than from a
dedicated store, so the catalogue covers whatever Debian carries and adding an application is one
line in a text file; the one application Debian does not carry, VSCodium, is a `.deb` taken from
its own releases page and installed by the same apt. Nothing in a box is downloaded prebuilt from
a KDOS server: each machine builds its own box images. And the confinement is aimed at the desktop
rather than at your files. A box shares your home directory by default; its windows reach KWin
through a socket tagged with the box's name, and what it cannot do, unless its box profile grants
it, is bind KWin's window-list, fake-input, screen-cast or clipboard-control interfaces, so by
default it has to ask a portal on the host.

The costs follow from the same choices. A store install needs a network and minutes of apt, is built
from unsigned registry content, and is larger than the same program would be on Alpine. On a session
booted from the install medium, a store install works, because the container store runs through
fuse-overlayfs, but it is held in memory and lost at power-off unless the session has a persistence
store; an imported pack box's writable layer is always held in memory there. And the box is a
packaging and desktop boundary, not a jail for a malicious application. See [Packs and
boxes](../03-architecture/packs-and-boxes.md) and
[Decisions](decisions.md#native-applications-on-the-medium-a-store-that-builds-the-rest).

## Updates

Debian and Fedora publish point updates and new releases from their archives, and `apt` or `dnf`
installs them into the running system. Arch rolls continuously. Gentoo syncs its tree and rebuilds
what changed. NixOS and Guix build a new system generation beside the old one and switch to it,
keeping earlier generations bootable. Silverblue downloads a new root image with rpm-ostree and
boots into it, keeping the previous one for rollback. ChromeOS and Android use two root partitions
and switch between them.

In KDOS a new version arrives as a release, because each recipe pins its version and a release pins
the whole tree. KDOS is therefore released rather than rolling: a machine moves when it takes a
release. Each release publishes, signed with the release key, the packages its build produced (a
binhost, with deltas against the release before) and whole-slot images of the root, with deltas from
the two releases before it. An update is written into the root slot that is not running, from the
image where the release carries one and from the binhost otherwise, while the running system is
untouched; then it asks for a restart. Discover offers it ("Restart to update"), through
`kdos-sysupdated`, which answers Discover's `sysupdate1` calls, and `kdos update apply` does the
same from a shell. This is KDE Linux's model of an operating system that is an image, applied to a
root built from packages; the reasons are in
[Decisions](decisions.md#updates-are-signed-slot-images-with-a-binhost-on-the-way).

The next boot tries the new slot. The initramfs, the small early-boot filesystem that finds and
mounts the root, counts attempts, and the slot is kept only when a boot of it has finished `rcS`
and had a Plasma session (or, where nobody logs in, the login screen) up and answering for a
minute; a trial that never gets that far falls back to the slot known to work. This is the ChromeOS
and Silverblue idea of an update that cannot break the running system.

Nothing installs updates automatically, and nothing checks the network on its own: the nightly
check compares what is installed with what is already on the machine, and a check that downloads
is one somebody starts. `kdos update apply --source` builds the release's signed recipe tree on the
machine instead of installing its binaries, which is Gentoo's model with a signed tree in place of
a sync, and without USE flags; it takes as long as compiling does, which is hours for a large set.
Slot trials have been booted on virtual firmware only, a second encrypted slot has never been
booted, and the update path has not been run against a published release over the internet. See
[Packaging](../03-architecture/packaging.md#updating-a-machine),
[Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection) and
[Known gaps](../06-reference/known-gaps.md#boot-and-updates).

## The security model

Fedora enables SELinux in enforcing mode by default, and Ubuntu and Debian enable AppArmor. The
major binary distributions boot under UEFI Secure Boot through a signed shim. Their archives are
signed, and their security teams publish advisories and ship fixes.

KDOS is a single-user workstation, and its security model is narrower and explicit about it. It
defends against applications misbehaving, against tampered artefacts, against escalation through the
few privileged programs, and against catastrophic mistakes made through its own interfaces.
Boxed applications run in rootless containers. A box's windows reach KWin through a socket tagged
with the box's name, and KWin denies a tagged client the desktop-shell interfaces (the window list,
fake input, screen casting, clipboard control and the rest), widened for one box only by an
explicit grant in its profile; the box reaches the session bus only through a filter that passes
the portals and little else. The root daemons obey callers by their credentials and never accept a
path from them. polkit decides what may be done without a password: the person in the session
elogind marks active at the machine gets what upstream grants an active local session, members of
`wheel` get KDOS's further grants for disks, networks and smart cards without a prompt, and
anything else asks for an administrator's password in Plasma's authentication dialog. Passwords
that applications save go to ksecretd, KDE's Secret Service, which the login password unlocks.
Host sources are verified by the hash in their recipe, updates and prebuilt packages by the
release's signatures, imported packs at install by their hash and, when they carry one, their
signature, and packs on the install medium by the same check the first time they are mounted. The
shipped image's setuid-root binaries all come from upstream ports; none of them is KDOS's own.
`kdos cve` compares installed versions against a vendored copy of Alpine's security database,
offline.

It is equally explicit about what it does not do. There is no SELinux or AppArmor, no Secure Boot,
no verified or measured boot, and disk encryption is a passphrase typed at boot rather than a key
sealed in a TPM. Membership of `wheel` is effectively root. A box shares your home directory. With
automatic login there is no password to unlock the secret store, so it asks for one the first time
an application uses it. There is no security advisory service and no automatic update. The release
key is the trust root for everything on the host: it signs the recipe tree and every binary built
from it. See [The security model](../03-architecture/security-model.md), and read
its last section, [What is not
protected](../03-architecture/security-model.md#what-is-not-protected), first.

## Hardware and support

The mainstream distributions support several processor architectures, test on a wide range of
machines, and in some cases offer commercial support and certified hardware. Alpine, Void and
Arch have community support and broad hardware coverage from the same upstream kernel and firmware.

KDOS builds for x86-64 alone. Its kernel is built from source with one configuration, and the
`linux-firmware` tree ships everything an x86 driver can load, which covers a great deal of
hardware, but nothing is tested against a wide range of devices and broad hardware enablement is
not a goal. The boot
loader is Limine, which is unsigned, so Secure Boot has to be turned off in the firmware. There is
no vendor, no support commitment, no migration guarantee between release lines and no tested
hardware matrix. See [Known gaps](../06-reference/known-gaps.md#hardware-and-platform) and
[Status](../06-reference/status.md).

## What you give up

Each section above states its own cost. Some are not stated elsewhere. The first build from a
clean checkout takes most of a day, and gigabytes more disk if it keeps phase snapshots; see
[Developing](../05-developer/developing.md#the-first-build). Commercial software that assumes
glibc and systemd on the host does not run there; it runs in a box or not at all. Of what a Plasma
user elsewhere has, four things are missing: Flathub, because the catalogue's boxes fill its role
from a list this repository keeps; "Get New…", because nothing downloads at run time; DrKonqi's crash reports,
because they need `systemd-coredump`; and Plasma's own login manager, because it needs systemd,
so the login screen is SDDM's. KDE's translations are not built either: the system is English
only. Whether the trade as a whole is worth it depends on what you want the machine for;
[Why KDOS](why-kdos.md#who-should-run-kdos) says who it suits.

## See also

- [Why KDOS](why-kdos.md) — what KDOS is and who it is for
- [Principles](principles.md) — the rules behind each difference above
- [Decisions](decisions.md) — the close choices and the alternatives that lost
- [Architecture overview](../03-architecture/overview.md) — the running system as a whole
- [Session](../03-architecture/session.md) — the login, elogind and turnstile, and how Plasma
  starts without systemd
- [Packaging](../03-architecture/packaging.md) — recipes, `kpkg`, the binhost and updates in depth
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the build from checkout to ISO, told
  end to end
- [The ports catalogue](../06-reference/ports-catalogue.md) — every port, by shelf
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — how applications are built and run
- [The security model](../03-architecture/security-model.md) — what is protected and what is not
- [Known gaps](../06-reference/known-gaps.md) — what does not exist

<!-- book-nav -->
---

*Part I — Introduction, chapter 2.* Previous: [1. Why KDOS](why-kdos.md) · [Contents](../README.md) · Next: [3. Principles](principles.md)
