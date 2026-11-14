# Decisions

This chapter records the choices in KDOS where a reasonable engineer could have gone the other way:
the desktop, its compositor, the login and the look, how software is found and the application
catalogue, the C library, the base userland and PID 1, where upstream sources are kept, how the
ports tree, KDOS's own code and the build phases are laid out, how CPU optimisation is chosen, how
the system updates and installs, and several smaller ones. It is written for readers who want to
know why the system has the shape it has, and for contributors about to propose a different
approach, whose objection may already have an answer here. Read [Why KDOS](why-kdos.md), [How KDOS
differs](how-kdos-differs.md) and [Principles](principles.md) first; the entries assume the goals
and rules set out there.

Each entry gives the conclusion first. Most then give the question it answers, the alternatives and
why they were not taken, and what the choice costs; an entry leaves a part out where there is
nothing more to say about it than the conclusion already does. The entries link to the chapters that
describe the mechanism as built. The final section, [Narrowings](#narrowings), collects smaller
decisions that look like missing features.

## Index

| Decision | Conclusion |
|---|---|
| [The desktop](#kde-plasma-and-all-of-gear-as-the-desktop) | KDE Plasma and all of KDE Gear that can run here, each family pinned to one release |
| [The compositor](#kwin-is-the-compositor) | KWin, configured and patched, never forked |
| [The login](#sddm-with-a-wayland-greeter) | SDDM, its greeter on `kwin_wayland`, and no X11 session |
| [The look](#kdos-modern-with-phosphor-mode-on-request) | KDOS Modern in eight accents from one palette; Phosphor mode off until asked for |
| [Windows](#floating-windows-with-smart-snap-tiling-on-request) | Floating windows with edge and quarter snap; an automatic tiler bundled and off |
| [Application delivery](#a-base-on-the-medium-every-port-prebuilt-beside-it) | The medium carries the base; every native port is prebuilt in the release's binhost or built on the machine; podman builds catalogue applications on request |
| [Finding software](#discover-with-kdos-backends-as-the-one-software-centre) | Discover, with KDOS backends for ports, packs, boxes and the root slots; no Flathub and no "Get New…" |
| [Application packaging](#one-pack-per-application-not-one-image) | One artefact per application over shared runtimes, never one image |
| [The base distribution in boxes](#debian-inside-boxes-not-alpine) | Debian trixie, with Alpine carried as a scratch base |
| [The host C library](#musl-as-the-host-c-library) | musl, which forecloses runtime CPU dispatch |
| [The base userland](#gnu-at-every-name-no-multi-call-userland) | GNU coreutils in full, grep, sed, gawk, findutils, diffutils, patch and tar; no toybox, BusyBox or uutils |
| [PID 1](#sysvinit-as-pid-1) | sysvinit 3.18 with runlevels; util-linux `agetty` and `sulogin`; elogind for logins and turnstile for a user's services |
| [Where upstream sources live](#upstream-archives-are-hash-checked-release-assets) | Numbered volumes of shelves, each file under `<shelf>--<file>` and checked by its sha256, fetched by `make fetch` |
| [CPU optimisation](#-march-measured-per-machine-not-chosen-for-a-population) | Measured per machine by `kdos march`, never a shipped feature level |
| [Compiler flags](#one-release-flag-set-raised-per-port) | `-O2` with hardening for every port; `-O3` per port on precedent; LTO only for a listed set of hot libraries |
| [The vulnerability database](#alpines-security-database-not-nvd-or-osv) | A vendored, pruned copy of Alpine's secdb, answered offline |
| [Recipe format](#the-build-shell-lives-beside-the-recipe) | Two files: parsed metadata, plus ordinary bash |
| [The ports tree](#ports-are-shelved-by-subject-identity-is-the-bare-name) | 102 subject shelves under `ports/core`; a port is its bare name, unique across the tree |
| [KDOS's own code](#kdoss-own-code-is-divided-by-what-each-program-is) | Six areas under `src/`, every port exactly two levels deep; only `src/desktop` links a toolkit, and only Qt and KDE's |
| [Build phases](#build-phases-are-named-banded-and-closed) | Thirteen named phases in bands of ten, split by the dependency graph; each list names exactly what its phase installs |
| [Binhost signing](#signing-the-index-not-every-package) | One signature over the index; a signature file holds several |
| [Updates](#updates-are-signed-slot-images-with-a-binhost-on-the-way) | The release's signed image, or its signed packages, written into the inactive root slot and kept after a healthy session; a source build on request |
| [The installer](#one-installer-backend-two-front-ends) | One backend under a Kirigami front end and a text one; Partition Manager for manual layouts |
| [The ASCII demo](#freezing-a-demo-rather-than-writing-one) | A frozen hard fork of the AA-project's `bb` 1.3rc1 |
| [Narrowings](#narrowings) | Small decisions that read like gaps |

## KDE Plasma and all of Gear as the desktop

The desktop is KDE Plasma, Wayland only, with KDE Frameworks under it and every module of KDE Gear
beside it that can run on KDOS. KDOS does not maintain a desktop of its own. It integrates Plasma:
the look, the defaults, and the settings pages, plasmoids, KWin plugins and Discover backends that
bring KDOS's own mechanisms (packs, boxes, the two root slots, the firewall) onto Plasma's own
surfaces.

The question was what a person sits in front of. A desktop written for KDOS was the alternative,
and its appeal is the one the rest of the system has: small enough to read whole. It was not taken
because most of what a desktop has to do is not specific to KDOS: window management, a lock screen
that survives its own crash, screen capture through portals, input methods, a screen reader,
several monitors at different scales, colour management, drag and drop, session restore. Each of
those is a protocol whose behaviour shows only under real applications and real hardware, Plasma
has had years of both, and everything KDOS wants to add fits the plug-in points Plasma already
has.

Plasma was chosen among the complete desktops for three reasons. Its 6.8 series runs on Wayland
alone, which is the display rule KDOS keeps anyway
([Principles](principles.md#no-xorg-server-and-one-carve-out)). It runs without systemd: Alpine,
Chimera Linux, Gentoo and Void ship it on elogind with inits of their own, so each part KDOS needs
has a working precedent, on musl among them. And its configuration is layered files, so KDOS's
defaults are a directory in the configuration search path, after the machine owner's `/etc/xdg`,
rather than patches to the programs that read them.

All of Gear is ported rather than a selection, because a machine with no network has only what its
medium carries ([Principles](principles.md#offline-by-construction)), and Gear's applications are
written against the same frameworks, so they share the look, the file dialogs, the shortcuts and the
settings without integration work per application. Four Gear modules are left out because they
cannot work here: kjournald needs libsystemd, kmag reads the screen in a way Wayland does not
allow, kmousetool needs X11, and plasma-settings is Plasma Mobile's settings shell. Thirteen of
Plasma's own modules are left out for the same kind of reason: the GRUB and Plymouth themes and the
Plymouth settings page (KDOS boots with Limine and its own splash), DrKonqi (it needs
`systemd-coredump`), the Flatpak settings page (there is no Flatpak), the KDE Frameworks 5
integration plug-in (no KDE Frameworks 5 are built), plasma-firewall (it drives firewalld or ufw
through systemd), the Plasma Login Manager (it needs systemd), and the Bigscreen and Mobile shells
with the phone dialer, the text-message client and the nano shell they share.

**One version per family.** Every Plasma module is at one version, every framework at one and every
Gear module at one, and a family moves in one change that rebuilds it, because Plasma's modules
require each other at the same version and Gear's PIM modules pin each other exactly. Plasma
follows its latest bugfix release, and a new minor release only once every KDOS plugin tied to it
has been rebuilt and passes its tests. KDE Frameworks moves when Plasma requires it or a fix is
needed, always as the whole set. Gear takes each point release of its series, and a new series
after that series' first bugfix release. A change that moves one module of a family alone fails
preflight. [Packaging](../03-architecture/packaging.md) gives the set the tree pins.

The cost is weight and a release lock. Qt, KDE Frameworks, Plasma and Gear are hundreds of recipes
and more code than the rest of the host, and they lengthen the first build by hours. KDOS's code
that uses Plasma's internal interfaces (the Discover backend, the System Monitor plugin and the KWin
effect) is rebuilt and tested again at every Plasma minor release. And KDE's translations are not
built: the image is English only, as the rest of the system is, while KDOS's own Qt code marks
every string for translation so that a translated release is a build flag rather than a rewrite.

## KWin is the compositor

The compositor is KWin, Plasma's own, built from its release tarball with the options KDOS needs
and a small number of carried patches. KDOS does not fork it. KWin's behaviour is set through its
configuration (defaults for `kwinrc`, `kwinrulesrc` and `kglobalshortcutsrc` in
`/usr/share/kdos/xdg`), through KWin scripts and effects, and through window rules; what none of
those reaches is a `.patch` with a `Subject:` line, offered upstream when it is written.

A fork of the compositor was the alternative, and it would let KDOS change anything. It was not
taken because a fork stops receiving upstream's fixes, and a compositor's fixes are security and
hardware fixes more often than features, each of which a fork's owner reads and applies by hand. A
patch is a smaller promise: at the next release it applies or it fails loudly, and it never drifts
in silence.

Two needs show where the line falls:

- **Phosphor mode's CRT pass** is a full-screen effect over every output, which only a C++ effect
  that paints the whole screen can be; KWin's scripted effects paint one window at a time. An
  effect built outside KWin records the exact KWin version it was built against, and KWin refuses
  to load it at any other version, point releases included, so a separately packaged effect would
  stop loading at the next bugfix release without a word. Built into KWin by a patch, the effect is
  compiled with every KWin and can only fail to build, never fail to load.
- **What a boxed application may bind.** KWin denies a client that arrives through a tagged
  security-context socket a short list of interfaces, among them window management, fake input,
  screen casting and clipboard control. KDOS's patch extends the list to the rest of the
  desktop-shell interfaces and lets a box's profile grant one back by name.

The cost is a rebase of every patch at every Plasma release, and a set of KDOS plugins tied to
KWin's internal interfaces that are rebuilt with it. Each patch's merge request is named in the KWin
port's notes, and a patch upstream accepts is dropped at the release that carries it.

## SDDM with a Wayland greeter

The login screen is SDDM. `init` starts it on the first virtual terminal once the boot splash has
gone dark; its greeter runs on `kwin_wayland` with no Xwayland, and it offers one session, Plasma
on Wayland. A login goes through PAM, where elogind records the session and turnstile starts the
user's services, and SDDM then starts Plasma. The second virtual terminal keeps a text login for
recovery, and when no SDDM can start, `init` falls back to a password prompt on the first one,
never to an automatic login.

A text login on the first terminal that starts the desktop from the shell profile was the
alternative, and it is one program fewer. It was not taken because Plasma's session expects a
display manager: Switch User asks it for a second greeter on a free terminal, logging out returns
to it, and its settings page is where autologin and the login screen's look are set.
Plasma's own login manager was the other candidate. It needs systemd, at build time and in its
greeter, and SDDM does not.

SDDM's Wayland greeter is the path both SDDM's upstream and Plasma's mark as experimental. KDOS
takes it because an X11 greeter would put an X server on the login path, which
[Principles](principles.md#no-xorg-server-and-one-carve-out) rules out. That is the cost: the login
screen runs on a code path few distributions ship, so every boot, login, logout, user switch, lock
and suspend through it is a test KDOS has to run itself.

## KDOS Modern, with Phosphor mode on request

The look is KDOS Modern: Plasma's Breeze widgets and window decoration, configured square and
painted in KDOS's colours, with a KDOS Plasma style, marks in the icon theme, wallpapers, a splash
and a login screen in the same style. There are eight accents, `bone` by default, and each is a
separately named KDE colour scheme and global theme. Every colour file, Konsole scheme, wallpaper
and login-screen palette is generated at build time from the one palette in `libkcolor`, the table
the boot menu, the splash and the text consoles take their colours from, so the machine wears one
accent from the firmware to the desktop. Every text colour of every generated scheme must clear
4.5:1 against the backgrounds it is drawn on.

Forking Breeze, the lock screen or the login theme was the alternative, and it gives the most
control. It was not taken because forked QML and third-party widget engines break at Plasma
releases, often as a lock screen that draws nothing rather than as a failed build. KDOS derives
instead: a package that has to differ from Breeze starts from the installed Breeze files at build
time and carries a patch with a `Subject:` line, so upstream drift is a build that fails.

Phosphor mode is an extra layer on KDOS Modern, off until somebody turns it on: a CRT pass over the
whole screen in the current accent, CRT motion for windows, the login, the lock screen and the
logout, and the retro accents. It is opt-in for three reasons. Its scanlines cost contrast; it
cannot run on software rendering, so a first boot in a virtual machine would look different from the
same image on hardware; and while it runs, the screen gives up direct scanout and the hardware
cursor plane. It is offered once, on the welcome tour, behind one switch.

## Floating windows with smart snap, tiling on request

Windows float where KWin places them, and snap: dragged to an edge a window takes half the screen,
dragged to a corner a quarter, and each virtual desktop keeps its own layout of custom tiles. An
automatic tiler, the Krohnkite KWin script, ships with the system and is off; choosing the tiling
desk on the welcome tour, or the one switch for it, turns it on.

Tiling by default was the alternative, and it suits a person who lives in terminals. It was not
taken because it surprises everybody who expects a new window to open where it can be seen whole,
and it turns every application's first window into a layout decision. One tiler is bundled, not
two, so there is one behaviour to document and test. The cost is a third-party script tied to
KWin's scripting interface, pinned to a release and tested again at every Plasma minor release.

## A base on the medium, every port prebuilt beside it

The installation medium carries the *base*: the ports [`ports/base`](../../../ports/base) names and
their dependency closure, which `script/lib/imageset.py` computes over each recipe's `depends =`
line without following it into a compiler `ports/buildonly` lists. That is the Plasma session, the
core KDE applications, Firefox ESR, networking, printing and scanning, the box stack, the
self-hosting toolchain and every program of KDOS's own, about 1,000 of the about 2,430 packages a
build makes. The build compiles every port all the same, and the release publishes every package
it made on its signed [binhost](../06-reference/glossary.md#binhost), so the rest of the native
ports (all of KDE Gear, LibreOffice, GIMP, Blender, Wine and the other applications
[The ports catalogue](../06-reference/ports-catalogue.md) lists by shelf) are one download away:
`kdos update install` takes a port or one of the groups `ports/groups` names, and Discover
offers each port under *KDOS system*. Any port can instead be built on the machine from its recipe
(`kdos update install --source`, or Discover's *KDOS, built on this computer*), with what it lacks
downloaded prebuilt or built too. `/var/lib/kdos/origin` records which way each package came, and
an update keeps it that way.

For everything KDOS does not port, the medium carries a catalogue, and podman builds what somebody
asks for. A catalogue row is a parent chain of apt packages: installing a row builds a podman image
per row in its chain, each `FROM` the one below, and creates a
[box](../06-reference/glossary.md#box) (a rootless podman container one application runs in) over
the top one. The catalogue file installs to `/usr/share/kdos/appstore/catalogue`, and it is what
Discover's KDOS backend, kinstall (the installer) and `kdos app` all read, so adding an application
to KDOS is one line in a text file. Discover lists the catalogue beside the native ports, and
kinstall offers it by group — seven of them, from one application each in `essential`, `creative`,
`make` and `games` to six in `science` — so choosing the six science applications is one choice
rather than six. kinstall offers the native groups on the same page, and installs them into the new
system from the binhost while it installs.

Carrying every native port on the medium was rejected: a whole-tree ISO is about 20 GB, five times
the base's, and most of it is software a given machine never runs. Building every port on the
machine that wants it was rejected as the default for the reason [the updates
decision](#updates-are-signed-slot-images-with-a-binhost-on-the-way) gives: a browser or an office
suite takes hours to compile. `make build KDOS_IMAGE=full` still writes a medium that carries every
package, for a machine that installs offline.

The base and the binhost have three costs.

1. Anything outside the base needs a network, or the binhost on a disk, to install.
2. A release is not whole without its binhost. Every release has to build, sign and publish every
   package beside the ISO, and a machine takes only the binhost of the release its recipe tree
   names, so the packages are tied to the base of the same tag.
3. The image lane of an update writes the base alone. It then installs the machine's world set (what
   was installed on purpose) and its missing dependencies into the new slot from the same release's
   binhost, or builds the ones that came from source; a prebuilt package the binhost does not cover
   refuses the image lane before the slot is written.

Carrying every catalogue application prebuilt on the ISO, as
[packs](../06-reference/glossary.md#pack) (the application images described below), was rejected: it
puts every application on every medium whether or not it is ever launched, costs an hour of building
to add one row, and needs a release channel to push the whole set through.

Building catalogue applications on request has three costs.

1. A store install is unsigned. It pulls its base image from a public registry and its packages
   from Debian's archive, and nothing in `/etc/kdos/keys` vouches for either. `kdos-box create`
   prints exactly that about an OCI base, and it is true of every application built this way. The
   claim that everything is verified holds for an imported set and for nothing else.
2. Installing needs a network, and minutes of apt.
3. A live session keeps less of what it installs. `$HOME` is on the boot overlay there, so the
   container store runs through fuse-overlayfs, and what a store install builds is written under
   `$HOME`, held in memory and lost at power-off unless the session has a persistence store (see
   [Getting started](../02-user-guide/getting-started.md#keep-what-you-change)). A pack box's
   writable layer is always a tmpfs there, persistence store or not, because the kernel refuses an
   overlay upper directory on overlayfs, so whatever a box writes outside the home directory is gone
   at shutdown. See
   [Known gaps](../06-reference/known-gaps.md#a-live-session-cannot-create-a-persistent-box).

That is why import and the pack format exist. `kdos-appbox export <file.ktar>
<id|group>...` writes the built images as packs with an index, signed when `KDOS_PACK_KEY` names a
readable key and saying plainly that the index is unsigned otherwise. `kdos-appbox import
<file.ktar> [<id>...]` stages each pack through `kdos-packd`, the root daemon that mounts packs,
which hashes and signature-checks it where it mounts it; with no names it imports the whole set the
archive carries. An imported application is better verified than a store-installed one and needs no
network. kinstall imports a set it finds on a stick, but into the live session rather than onto the
new disk (see [Installation](../02-user-guide/installation.md#8-applications)). The pack format is
how a set is carried, not how software is distributed.

## Discover, with KDOS backends, as the one software centre

Discover is where software is found, installed, removed and updated. KDOS builds it with a backend
of its own for the native ports, the catalogue's packs and boxes, the boxes already installed, and
packs on an attached medium or opened from a file; with its `sysupdate1` backend, which
`kdos-sysupdated` answers for the two root slots; and with its fwupd backend for firmware. The
Flatpak, Snap, PackageKit and KNewStuff backends are not built, and there is no Flathub: every
source Discover offers is described in this repository, and every native one is built from it
([Principles](principles.md#offline-by-construction)). A native port that is not installed is
listed once with two origins, *KDOS system* (prebuilt) and *KDOS, built on this computer*, and the
catalogue's boxes fill Flathub's role under KDOS's own confinement.

A software centre of KDOS's own was the alternative, and it would understand packs and boxes with
no translation. It was not taken because it would be a second place to look, beside the one every
Plasma user already knows and every update notification already opens.

The cost is twofold. The KDOS backend is written against Discover's internal programming
interface, so it is rebuilt and tested again at every Plasma minor release. And Discover shows
nothing that has no AppStream metadata, so the image build generates that metadata for the ports,
the packs and the catalogue; a port, pack or box without it is invisible there, though it still
installs from the command line.

## One pack per application, not one image

Each application is its own artefact: a podman image per catalogue row when the store builds it,
built over a small set of shared [runtimes](../06-reference/glossary.md#runtime) (layers holding a toolkit
or a family of libraries that many applications sit on), and one flattened
[pack](../06-reference/glossary.md#pack) per chosen application when a set is exported. Installing
an application disturbs nothing else, and in the store a shared runtime's image is stored once
however many applications sit on it.

The question was how the catalogue's 70 applications (the `app` rows in
`src/system/kdos-appbox/catalogue`) reach a machine: as one container image, or as separate
artefacts.

A single image was rejected because it puts every application on every install whether or not it
is ever launched, and it turns adding one application into a rebuild of the whole image.

The alternative also hits two limits that have nothing to do with size. Overlayfs cannot add a layer
to a mounted overlay, so installing an application into a shared box would mean restarting a
container other applications are running in. And a hundred lower directories do not fit in the 4096
bytes `mount(2)` allows for its option string. Per application, a store-built stack is two to four
images — the base, none, one or two runtimes, and the application, each `FROM` the one below — so a
runtime's files are one copy on disk under every box built over it. An exported pack gives that
sharing up: it holds its base and runtimes as well as the application and requires nothing, so it
can be imported onto a machine that has none of them.

The cost is one `conmon`, podman's container monitor, per running application rather than one for
all of them, and only for applications that are open. See
[Packs and boxes](../03-architecture/packs-and-boxes.md#where-a-pack-comes-from) for what an export
holds and [Composition](../03-architecture/packs-and-boxes.md#composition) for how an imported box's
root is assembled.

## Debian inside boxes, not Alpine

The application catalogue is built on `debian:trixie-slim`, with every row installed from a pinned
Debian snapshot archive.

The question was which base distribution the catalogue should use. KDOS itself uses musl as its C
library and would pair naturally with Alpine.

The catalogue decided it. Its purpose is to offer the strongest free application in each segment
it covers — office, creative work, development, science, CAD and 3D printing, games — and Debian
packages and patches nearly all of them, where Alpine's repositories lack a number of them. The one
application the catalogue carries that Debian does not package, VSCodium, is fetched as a `.deb`
from its own releases by a `deb` line in the catalogue, and apt resolves its dependencies. An
all-musl system would be more consistent and its images far smaller, and neither buys anything for
a user who wants to open a CAD file.

The catalogue's `snapshot = auto` line pins the Debian archive to the date the base image itself
was built from, so the packages installed on top cannot disagree with the root filesystem under
them. A literal timestamp pins it explicitly, and `off` reads the live archive and gives up
reproducibility.

Alpine is present all the same, as a second base pinned to `alpine:3.24.1`: about 3 MB of busybox
and musl, with no packages added. A clean scratch userland that needs no network is worth one row,
and Alpine is the one non-Debian root filesystem small enough that carrying it costs little.

The cost is size, and it is accepted: a Debian box is much larger than the same program would be
on Alpine. Every generated `apt-get install` passes `--no-install-recommends` to keep it from
growing further.

## musl as the host C library

The host C library is musl.

The question was which C library the host is compiled against, and the alternative was glibc, which
the Debian base of the boxes uses. musl was chosen because it is small, readable and close to the
standards, which suits a system meant to be understood in full.

musl closes some doors, and they are stated here rather than discovered later. It has no
`glibc-hwcaps` mechanism, so runtime dispatch for CPU optimisation — building several variants of
a library and letting the loader pick one — is not available, which is one reason
[`kdos march`](../04-programs/kdos-command.md#kdos-march) measures per machine instead. Some
upstream code assumes glibc extensions and needs a build flag or a patch. And a header warning that
glibc does not emit can turn an upstream `-Werror` build fatal.

The application catalogue is glibc, inside boxes built on the Debian base. That is the purpose of
the [ring](../06-reference/glossary.md#ring) boundary: the host and the boxes need not share a C library.

## GNU at every name, no multi-call userland

The base userland is GNU's: coreutils in full, as separate programs, with grep, sed, gawk,
findutils, diffutils, patch and tar, cross-built in `10_bootstrap` and rebuilt as ports in
`30_foundation`. util-linux, procps-ng and iproute2 supply the system tools.

The question was whether a compact multi-call userland could serve as the base, as BusyBox does
for Alpine. The alternatives were toybox, BusyBox, uutils, chimerautils and sbase. The deciding
property is how a tool fails. A compact tool that differs from GNU tends to differ silently: an
`install -m644` that writes 0600, an `awk` that prints nothing, a `grep` that matches nothing and
reports no error. On a system that compiles every package, each such difference is a build that
goes wrong without failing, and the wrong output reaches the image. BusyBox on musl gives different
answers from GNU on about a third of a probe set, several of them wrong with no error, and its
current releases are labelled unstable; uutils needs Rust, which the build has only after its
compilers phase. GNU coreutils passes its own test suite on musl.

What this costs is a longer first build stage and a few megabytes on the image. What it buys is
that every configure script meets the tools upstream tested against, and that a build which
passes produced what it says it did. See
[Packaging](../03-architecture/packaging.md#the-base-userland).

## sysvinit as PID 1

PID 1 is sysvinit 3.18: `init`, `telinit`, `halt`, `poweroff`, `reboot`, `shutdown`, `runlevel`
and `killall5`, and nothing else from that package. Every console is util-linux's `agetty`, and
single-user mode is util-linux's `sulogin`.

The question was which init runs the numbered shell scripts, with systemd ruled out by
[principle](principles.md#no-systemd-init-units-or-service-manager). sysvinit gives runlevels, so
single-user mode, power-off and reboot are states of `/etc/inittab` rather than code paths of their
own; it builds on musl and against utmps with no patch, so it writes the boot and runlevel records
`who -b` and `who -r` read and the `/run/runlevel` file `runlevel` falls back on; and its shutdown
order is a list of `wait` entries anyone can read. It does not supervise services: that stays
`ksvc`'s job, so the choice of PID 1 changes nothing about how a daemon is run.

The constraint it brings is that `telinit` is the only way to ask it for a runlevel. It ignores
`SIGTERM`, and `SIGUSR2` closes its control FIFO for the rest of the boot, so a program that wants
the machine off runs `telinit 0` and never signals PID 1. See
[Boot and init](../03-architecture/boot-and-init.md#shutdown) for how the machine is taken down, and
[Login records](../03-architecture/boot-and-init.md#login-records) for the records `init` writes.

sysvinit has no notion of a login, and Plasma is built on one, so **elogind** runs beside it:
logind, taken out of systemd's source tree and built on its own. It serves
`org.freedesktop.login1`, through which SDDM records sessions and switches users, KWin opens the
display and the input devices, PowerDevil suspends the machine and owns the lid, and the lock screen
locks before sleep. It runs under `ksvc` like any daemon, and its power-off and reboot paths are
sysvinit's, so a Shut Down from Plasma still goes through runlevel 0 and `rcK`. **turnstile** runs
beside elogind for the part of `systemd --user` Plasma needs: it counts a user's logins across SDDM
and the text consoles, creates `/run/user/<uid>` at the first and removes it at the last, and has a
backend start that user's services. On KDOS the backend is a short script that runs
`/etc/kdos/user-services/` under `ksvc`, so a user's session bus and PipeWire are supervised exactly
as the system's daemons are.

systemd itself was the obvious way to get all of that, and it was not taken for reasons beyond the
principle. Its upstream calls its musl support incomplete and makes no promise to keep it; from
version 260 it runs no SysV scripts, so every script in `/etc/init.d` would have to become a unit;
and it would make one project of the journal, the units and the user manager, jobs this system
gives one program each. dinit, on which Chimera Linux runs Plasma with elogind and
turnstile, was the other candidate. It would replace both sysvinit and `ksvc` with a service manager
that orders services by a dependency graph, and gain nothing Plasma needs: turnstile asks its
backend only to start a user's services and to say when they are ready.

The cost is about 226,000 lines of C from systemd's tree on the host, built with the patches
Alpine carries for musl, and pinned: elogind moves to a new release only with its patches proven
again through a full boot, login, logout, user switch, lock and suspend.

## Upstream archives are hash-checked release assets

Upstream source archives are not kept in git. Every source file a recipe names by a `sha256 =`
line is a release asset in the GitHub repository `kunaldawn/kdos`. The exception is a small set of
upstream files that git does carry, such as bash's and readline's patch levels, the IANA
registries, `certdata.txt` and a Tesseract language model; those are never archived. The archive
is a run of numbered volumes, pre-releases tagged `sources-1`, `sources-2` and on, and each shelf
of `ports/core` has its volume, the second field of its line in `ports/shelves`. A file goes into
the volume of the first shelf, in the tree's sorted order, whose recipe names it, under the name
`<shelf>--<file>`; a file only old history names is kept as `attic--<file>` in the highest
volume. The asset carries the shelf and the file's own name, so a file's address reads like what it
is:

```
https://github.com/kunaldawn/kdos/releases/download/sources-1/archiver--zstd-1.5.7.tar.gz
```

The committed file `ports/sources.idx` records the release and the asset for each hash. The
current recipes name 2,572 distinct files, 39.3 GiB in total: 44 that git carries, and 2,528 that
belong in the archive, spread over 102 shelves. The four volumes hold 596 to 695 assets each, a
split file's parts counted, under GitHub's 1,000 assets per release. Sixty-one of the archived
files exceed the 100 MiB a push to github.com refuses; they appear 76 times across the port
directories, because a file such as the LLVM source tarball serves several ports. The largest,
`texlive`'s texmf tree, is 4.96 GB, over GitHub's 2 GiB limit per asset, so any file larger than
1,900 MiB is stored as `<asset>.part01` onwards and joined by `ports/fetch`; the texmf tree and the
Chromium source tarball are stored that way. `KDOS_SOURCES_REPO` names a different archive
repository, and `KDOS_SOURCES_BASE` a different download base; setting `KDOS_SOURCES_BASE` empty makes `make fetch` skip the archive and
go from the local cache straight to upstream.

The hash is the identity and the name is advisory. A recipe names contents, not a location, so a
file that verifies is the file the recipe meant whether it came from the archive, from upstream or
from a mirror added in ten years, and none of those changes a commit. A readable name is what lets
a person browsing a release, or reading a URL in a log, see which file it is; the hash already
guarantees which bytes it is. Two upstream files of different bytes under one filename on one shelf
would meet in one volume, so the second is stored as `<shelf>--<port>--<file>`, and a third as
`<shelf>--<port>--<hash12>--<file>`. GitHub rewrites every character outside `[A-Za-z0-9._-]` in an asset
name to a dot, so the index records the name GitHub returned rather than the one asked for, and
each asset's label keeps the true name for the release page.

Volumes keep the release page short: four archive releases rather than one per shelf, each a
readable run of neighbouring shelves in sorted order, its title naming the first and the last. A
volume is planned to about 650 assets, so it has room for the versions to come before GitHub's
limit of 1,000. The mapping is committed and never changes for a shelf that has one:
`ports/publish --plan` gives a volume only to a shelf without one, packing those into the highest
volume and then new ones, so no planning run moves a file. A file whose volume is full goes to the
lowest-numbered volume with room, or opens the next volume, and the index records where it went; a
later `--rehome` brings it home once its volume has room. The index is what the naming costs: one committed line per file,
`<hash> <tag> <asset> <port>/<file>`, with `parts=<N>:<h1>,…` for a split file, which
`ports/publish` writes only after GitHub reports every uploaded asset's digest equal to the bytes it
sent. Its first line, `# kdos-sources-index 2`, is its format; an index without it is not read at
all, so a checkout with an index of another format fetches from upstream rather than misreading an
address. The newest index says where every archived file is now, and an old checkout can be fetched
with it (`ports/fetch --tree <dir>`).

The archive is append-only by default. A verified asset is never replaced, because replacing one
would silently change what an old commit builds; a checkout from five years ago finds the bytes it
was written against after upstream has moved or gone. Two maintainer commands remove assets, and
both say so by name. `ports/publish --rehome` moves a file whose port has changed shelf into the new
shelf's volume and name, and a file that spilled back into its own volume, and deletes the old copy
only once the new one is verified and both the working and the pushed index place the file
elsewhere. `ports/publish --orphans --prune=yes-delete` deletes
files no current recipe names, sparing every file a recipe at any `v*` tag names or a freeze list
under `build/freeze/` holds; a checkout older than the pruning fetches those files from upstream.
For each KDOS release, `ports/publish --freeze <tag>` attaches `sources.sha256`, the list of every
hash that tag's recipes name, to the release of that tag, creating it as a draft when it does not
exist. The list is a convenience, one file that says what the release needs. What pins the hashes is
the tag itself: its recipes carry them, and git cannot change those without changing the tag.

The archive's releases share the repository's release page with the KDOS releases. Each volume's
notes hold a section per shelf, with the shelf's description and every file of it the volume holds,
with version, size and hash, and each is a pre-release created with `make_latest` off, so "latest" always means a KDOS system release.
GitHub's immutable releases stay off on the repository: the setting applies to every release in
it, and it would freeze an archive release at its first publication, after which no source could be
added to it.

The first cost is that a clone alone does not build. `make fetch` has to run once after a clone
and again after a recipe changes, and it is the only build step that uses the network. It takes
each file from the local cache or the archive, and falls back to the upstream URL in the recipe's
`source =` line, so a file missing from the archive, or an archive that cannot be reached, costs a
download from upstream rather than a failed build, as long as upstream still has the file. See
[Where sources come from](../05-developer/developing.md#where-sources-come-from) for the order it
looks in, and [How KDOS is built](../05-developer/how-kdos-is-built.md) for where `make fetch`
sits in the build as a whole.

The second cost is on the publishing side. A new or bumped source has to reach the archive, by
`ports/publish`, before the commit naming it is pushed, or that commit builds on the machine that
wrote it and nowhere else. The pre-push hook in `script/hooks/` enforces this by refusing a push
whose recipes name a hash the archive cannot be shown to hold. See
[Writing ports](../05-developer/writing-ports.md#publishing-sources) for the procedure.

Git LFS would make a clone the whole input to a build, and it is not used because the sources do not
fit in it. A free account has 10 GiB of LFS storage and 10 GiB of monthly bandwidth, shared across
every repository the account owns. The current sources take 39.3 GiB, nearly four times that
storage, on their own, before any older version the history names, and a single clone uses most of a
month's bandwidth. Past the allowance LFS reads are blocked outright, not slowed, so a repository
that depends on LFS stops checking out. Release assets carry no total-size or bandwidth limit and
allow 2 GiB per asset, and the files near or over that are stored in parts. Plain git objects are
not possible at all, since sixty-one of the files exceed the push limit.

## `-march` measured per machine, not chosen for a population

CPU optimisation is measured on the machine that will run the result.
[`kdos march`](../04-programs/kdos-command.md#kdos-march) builds a port twice, once with the
highest x86-64 feature level the CPU's `/proc/cpuinfo` flags allow, runs that port's own benchmark
against both builds, and keeps the flags only where the median win exceeds 3 per cent plus the
machine's own measured noise. The verdicts go to a ledger,
`/var/lib/kdos/march.ledger`, which `kdos march report` prints: kept, reverted and unmeasurable.

A port with no `bench =` line in its recipe is unmeasurable, never a winner; six ports under
`ports/core` carry one. Each measurement takes the median of five runs by default, because a single
run measures the scheduler and a mean measures the worst outlier. The noise floor is the larger
spread of the two builds' own runs, so a busy or thermally throttled machine has to see a larger win
before it keeps the flags.

Shipping a feature level was rejected because published benchmarks of `x86-64-v3` show both gains
and regressions: some programs run faster and others slower, and some draw more power for the same
work. A distribution that shipped v3 everywhere would ship those regressions and never learn of
them. Runtime dispatch, which would choose between variants at load time, is not available on musl
(see [musl as the host C library](#musl-as-the-host-c-library)).

Because every KDOS machine carries its compilers and its recipe tree, rebuilding per machine is
available here in a way it is not for a binary distribution. That turns "which flags" into "did they
help on this machine", which is a question with a measurable answer.

## One release flag set, raised per port

Every port from `20_selfhost` on builds with one set of flags, from `script/env/chroot.env`:
`-O2 -pipe -fPIC -fno-semantic-interposition -fstack-clash-protection -D_FORTIFY_SOURCE=2`, with
`-std=gnu11` for C, `-D_GLIBCXX_ASSERTIONS` for C++ and `-isystem /usr/include/fortify` for both
(the fortified headers described below), linked with
`-Wl,-O1,--sort-common,--as-needed,-z,now,-z,pack-relative-relocs`. Go builds with
`-buildmode=pie`. The whole set, with the
defaults for CMake, cargo and Go, is in
[Writing ports](../05-developer/writing-ports.md#the-release-flags).

`-O2` is the level Arch, Fedora, Debian and Gentoo build with. Alpine's base is `-Os` and T2 SDE's
is `-Os` with `-O2` for hot code; both raise single packages rather than the whole tree. `-O3`
across every port means larger code and more exposure to undefined behaviour in code nobody here
has tested at that level, for a gain that is mostly vectorisation, which on the baseline SSE2
target is small. So `-O3`, LTO and PGO are per port: a recipe raises its own level where upstream's
default or Alpine's recipe does, and LTO is for a short list of hot code only: the media codecs
and ffmpeg, zstd and lz4, pipewire, pixman, cairo and harfbuzz, python and Pillow, and cmake's own
build. The list is in [Writing ports](../05-developer/writing-ports.md#every-port). CMake's
Release `-O3` is left in place, because it is the configuration upstream tests. The one level below `-O2` is in the bare-metal cross compilers: the `libgcc` and `libstdc++`
they build for a microcontroller are compiled `-Os`, as Alpine's are, because that code is linked
into flash measured in kilobytes.

The global `-fPIC` keeps static archives linkable into shared libraries, but under it GCC may not
inline an exported function into a caller in the same file, since a preloaded library could replace
it. `-fno-semantic-interposition` gives that inlining back; the cost is that `LD_PRELOAD` cannot
replace a library's call to a function defined in the same source file. Calls between files, and
exported data, still go through the PLT and GOT.

What is ruled out, and why:

- **No feature level above x86-64 v1**, in any language: that is
  [`kdos march`'s](#-march-measured-per-machine-not-chosen-for-a-population) question, per machine.
- **No `-Ofast` or `-ffast-math` from the tree**: they break IEEE and `errno` semantics that
  numerical, database and audio code depends on. Where upstream chose one for its own code, as
  goxel, Hydrogen, Sauerbraten, Routino and SPLAT! do, the recipe keeps upstream's choice after the
  exported flags: that is the configuration upstream tests.
- **No global `-DNDEBUG`**: some headers change a structure's layout under it, so a library and its
  consumer built with different settings would disagree. CMake's Release sets it for its own
  project; a meson port keeps its assertions unless it is one of the few hot ones listed in
  [Writing ports](../05-developer/writing-ports.md#meson).
- **No global `RUSTFLAGS`**: it replaces a project's own `.cargo/config` flags. No global
  `panic=abort`, which changes what `catch_unwind` does.
- **No global `-Werror`** of any kind: it gains nothing at run time and fails ports hours into a
  build.
- **Hardening is not traded for speed**: default PIE and SSP from the compiler, stack-clash
  probes, a read-only GOT, fortified C library calls through the `fortify-headers` port (musl has
  no `_FORTIFY_SOURCE` of its own) and libstdc++'s assertions stay on every C and C++ port, and Go
  executables are PIE. Where the tree's flags do not reach is measured, not assumed: Go has no
  stack protector or fortification, GHC links its compiler and programs with `-z,now` but without PIE (its static
  libraries are not position-independent, so a `-pie` link fails, and only a `-dynamic` program
  links as PIE),
  Rust has no stack protector by design, yices2, wine, valgrind, LuaJIT (texlive's `luajittex`)
  and mednafen turn the stack protector off in their own builds, and what `00_cross` and
  `10_bootstrap` build, `kpkg` among it, is not fortified. The table is in
  [Writing ports](../05-developer/writing-ports.md#the-release-flags).
- **No control-flow protection** (`-fcf-protection`, Intel CET): the instructions cost nothing on
  an old processor, but the shadow stack needs the C library to enable it per process and to
  handle it across `setjmp`, signals and threads, and musl does neither. Indirect-branch tracking
  alone protects nothing without a kernel that enforces it for user space. Revisit when musl
  enables user shadow stacks.
- **`00_cross` and `10_bootstrap` keep their own plain flags**, from `script/env/host.env`: a flag
  the cross toolchain mishandled would break the compiler every later phase is built with. The
  exception is `kpkg` and `kinstall`. No package phase rebuilds them, so the binaries
  `10_bootstrap` compiles are the ones that ship, and their compile lines carry the tree's
  hardening themselves: `-fstack-protector-strong -fstack-clash-protection` and
  `-Wl,-O1,--as-needed,-z,now,-z,pack-relative-relocs -Wl,--build-id=sha1`.

## Alpine's security database, not NVD or OSV

[`kdos cve`](../04-programs/kdos-command.md#kdos-cve) answers from a vendored, pruned copy of
Alpine's security database, `src/system/kdos-tools/secdb/secdb.txt`, installed to
`/usr/share/kdos/secdb.txt`. It is about 260 KiB: 4,099 fix records for 798 packages, merged from
twelve Alpine branches (`main` and `community` for v3.19 to v3.24) by `secdb/vendor.py`. It is
committed and diffable, so the answer needs no network.

The question it answers is narrow and exact: is the version KDOS pins older than the version
Alpine records as fixing a CVE on the branch KDOS pins. That is a version comparison with libkpkg's `kp_vercmp`, the same
comparator `kdos-portup`, the port-version checker, uses, not a scan of binaries.

NVD was rejected because its data feeds are retired in favour of an API, which needs a network;
OSV because it carries the same facts in a far larger download. Alpine is the closest
distribution to KDOS that publishes machine-readable security data: musl, the same upstream
tarballs, comparable version pins.

The limit is stated with every run. The database is keyed by Alpine package names, so a port
named differently needs a `secdb = <alpine-name>` line in its recipe, and a CVE fixed in a version
Alpine never shipped is invisible. A package with no entry is reported unknown, never clean. The
data ages, so `kdos cve` warns once it is more than 180 days old, and `ports/update --cve` is the
separate online cross-check. See
[Packaging](../03-architecture/packaging.md#vulnerability-tracking) for the database as built.

## The build shell lives beside the recipe

A port is two files. `kpkgbuild` is declarative metadata that is parsed and never sourced;
`build.sh` beside it is ordinary bash. `bash -n`, shellcheck, syntax highlighting and `git diff`
all work on it, and no parser has to understand shell.

An argument-vector list with no shell at all would fit most ports, but a minority need heredocs,
loops, redirects, globs and command substitution, and a format that cannot express those cannot
build them.

Embedding a shell interpreter in `kpkg`, the host package manager, was rejected because there is no
embeddable evaluator: the candidates are either parsers that do not execute, or programs with their
own `main()`, which would mean vendoring tens of thousands of lines of third-party C into a tree
that vendors almost none. It would gain nothing either, since bash is in the sysroot (the tree the
host is compiled into) before `kpkg` is compiled and ships on the target regardless.

A single file with the shell carried inline under INI-style section headers was rejected on a
concrete case. The `rust` port's `build.sh` writes a `config.toml` through a heredoc whose body
contains a line reading exactly `[build]`, the start of a TOML section. A recipe format with
`[build]` as one of its own section headers would read that line as the start of its own section.

See [Writing ports](../05-developer/writing-ports.md) for the format as built.

## Ports are shelved by subject; identity is the bare name

The upstream ports, about 2,400, are filed on 102 **shelves**, one directory per subject:
`ports/core/<shelf>/<name>/`, such as `ports/core/kde/kwin/` or `ports/core/games-board/kpat/`.
The shelf list is closed. It is the file `ports/shelves`, one line per shelf giving its id, the
source-archive volume that keeps its files, and what belongs on it, and a port may sit only on a
shelf that file lists. A port's identity is its
bare name: the shelf is where the recipe is filed and nothing more.

The question was how a person finds, reviews and places a port among some 2,400. A single flat
directory answers none of those: `ls` shows thousands of names in alphabetical order, and nothing
says that `kpat` and `gnome-mines` are the same kind of thing or that a new card game belongs beside
them. Subject directories answer all three, in the way T2 SDE's `package/<repository>/<name>/`
does.

Several details follow from what the rest of the system already assumes:

- **The shelves sit under `ports/core`, not directly under `ports/`.** `ports/` also holds the
  fetch, publish and update tools, the source index and the caches. `ports/core` is the repository
  root: every `PORT_REPO` value and the `/etc/kpkg.conf` default name it, as `/ports/core` inside
  the chroot's bind of `ports/`, and never a shelf inside it.
- **The shelf comes from the path alone. There is no `shelf =` recipe key.** A recipe's bytes are
  its [recipe hash](../03-architecture/packaging.md#e--the-recipe-hash), so a new key in every
  recipe would change every hash and, under strict recipe matching, rebuild the whole tree. A key
  would also be a second statement of the shelf that could disagree with the first.
- **Everything names a port by its bare name.** `depends =` lines, the package lists, the package
  database, the binary host's index, delta names and `ports/sources.idx` all say `kwin`, never
  `kde/kwin`, so moving a port to another shelf is a `git mv` plus moving its name to that
  shelf's file in its phase's package list, and changes no hash and no package. The same holds for prose: a comment or a page that means a port names the
  port, not its shelved path, since a path goes stale the day the port moves; a path appears only
  to show the layout itself.
- **A name is unique across the whole tree, and a duplicate is an error, not a choice.** `kpkg`
  looks a name up flat and then on every shelf of a repository, and a second match inside one
  repository stops it with both paths named, as T2's tools abort on a name found in two trees.
  Picking the first would build whichever copy sorts earlier, with nothing saying so. The pre-push
  hook and preflight refuse a duplicate across `ports/core` and the `src/` areas before it is
  pushed.
- **`kpkg` descends into the shelves itself; they are never listed on `PORT_REPO`.** That list
  holds at most eight repositories, and a hundred shelves would not fit on it.
- **A shelf's name is constrained.** It is lowercase letters, digits and `-`. It is never `libs`:
  a source-less port hashes `../../libs` from its directory, which for a core port is
  `ports/core/libs`, and a shelf by that name would be hashed whole into `containers-common` and
  `musl-ldd`. It is never `core`, and never the name of a port, because a shelf named `perl` or
  `llvm` could not be told from a port of that name at the top of `ports/core`.
- **A `group =` family shares a shelf,** so the version checker's joint bump touches one directory.
- **KDOS's own ports are not shelved.** They keep their own areas under `src/`, for the reason in
  the next entry.

The shelves are chosen by rules applied in order, the first that matches deciding: a `group =`
family stays together, a name-prefix family (`python3-*`, `qt6-*`, `libretro-*`, `font-*`) decides,
KDE Frameworks go to `kf6`, data and plug-ins follow the program they exist for, an application is
filed by what a person does with it and never by its toolkit or desktop project, and a library goes
to its domain shelf when it has one. [Writing ports](../05-developer/writing-ports.md#choosing-a-shelf)
gives them in full.

A single recipe key naming a category, read by the tools, was rejected for the hash reason above.
Listing each shelf on `PORT_REPO` as a repository of its own was rejected because of its eight-entry
limit, and because the first-wins rule between repositories would turn a duplicate into a silent
choice. Filing applications under their toolkit or desktop project (a `kde-apps` or `gnome-apps`
shelf) was rejected because a person looks for a file manager or a card game, not for the library
it links.

The package lists are grouped by the same shelves, so there is one classification of the ports
rather than two that drift apart. See
[Packaging](../03-architecture/packaging.md#phases-package-lists-and-shelves).

The cost is judgement. Some ports fit two shelves, and a rule has to pick one; where the rules do
not settle it, the shelf of the port's only main consumer wins, or failing that the shelf a person
would open first. A name also has to be unique across every one of the ports, which one flat
directory would enforce by construction and which, across shelves, the tools check. And every tool
that walks the tree descends exactly one level: a walker that did not would see no ports at all and
report success, which is why `kpkg` warns about a repository that holds no port at either depth.

## KDOS's own code is divided by what each program is

KDOS's own software lives in six areas under `src/`, each one kind of thing:

| Area | Holds |
|---|---|
| `src/desktop` | KDOS's part of the Plasma desktop: Qt 6 and KDE Frameworks code (settings pages, plasmoids, the KRunner runner, KWin effects and scripts, the Discover backend, the look-and-feel packages, the installer's graphical front end) and the small C helpers the session runs, which link no toolkit |
| `src/daemons` | Root daemons whose client is the desktop account |
| `src/system` | The package manager, the `kdos` command and its services, packs and boxes and the socket that tags a box's windows, the installer's backend and text front end, and the two books as installed documentation; `kdos-kpkg` is built by script |
| `src/art` | Themes, pictures, sounds and the programs that generate them |
| `src/libs` | The nine `libk*` libraries |
| `src/devtools` | Programs that run on the build machine and are never installed: `kdosbuild` and `kdos-portup` |

A new program goes to the first area that fits: a library to `src/libs`, a program not installed on
the target to `src/devtools`, code that is part of the Plasma desktop or that only the graphical
session runs to `src/desktop`, a root daemon for the desktop account to `src/daemons`, a picture,
theme, sound or generator to `src/art`, and anything else to `src/system`, box infrastructure
included. Only `src/desktop` may link a toolkit, and only Qt 6, KDE Frameworks and Plasma's own
libraries; preflight refuses any other `src/` port that links one
([Principles](principles.md#the-desktop-is-kde-plasma)).

Every port sits exactly two levels below `src/`, at `src/<area>/<name>/`. That depth is not a
style: every recipe that compiles a `libk*` library finds it at `$PORT_SRC/../../libs`, the recipe
hash of every source-less port walks the same path, and `kdos-installer` compiles a file from its
sibling `kdos-appbox`. A third level would break all three.

The areas also carry the build's phase split. `40_lang` to `44_apps` and `60_kernel` search
`src/system` and `src/art`, and only `50_desktop` adds `src/desktop` and `src/daemons`. KDOS's
settings pages, plasmoids, KWin plugins and Discover backend link libplasma, KWin and libdiscover,
which `50_desktop` builds, and the daemons serve the session it assembles, so no userland port can
come to depend on either: the phase building it could not resolve the name. That makes five
repositories in the desktop phase, under the eight `PORT_REPO` holds.

Flattening every program into `src/<name>/` was rejected: `../../libs` would stop resolving, the
libraries would sit among the programs, and the phase split would have nothing to hang on.
Grouping the libraries by kind was rejected because their `libk` prefix already namespaces them,
and moving them would change every `-I` flag in every build script that uses one and every path the
recipe hash walks, for no gain. Shelving KDOS's own ports under `ports/core` with the upstream ones
was rejected for the depth reason: a shelved port's `../../libs` is inside `ports/core`.

## Build phases are named, banded and closed

The build's phases live in `script/phases/`, one directory each, named for what they build and
numbered in bands of ten: `00_cross`, `10_bootstrap`, `20_selfhost`, `30_foundation`,
`31_compilers`, `40_lang`, `41_system`, `42_graphics`, `43_toolkits`, `44_apps`, `50_desktop`,
`60_kernel` and `70_image`. Each directory holds its steps or its package list and its own
environment, `phase.env`. Steps inside a phase are numbered with gaps too, such as
`10_bootstrap/060_gzip.sh`, `061_tar.sh` and `062_coreutils.sh`, so their order is the number and
never an accident of the alphabet.

The numbering with gaps means a phase can be added between two others without renaming either,
and so without renaming every snapshot, log directory and page that names the later one. The names
say what a phase builds, so `--phases 43_toolkits` reads as what it does.

**The userland is split by the dependency graph, not by subject.** Each phase's boundary is a
property of every port's dependency closure: `31_compilers` holds the large compilers (LLVM and
Clang with their runtimes, Rust, Go, GHC, Zig, Node.js and Ruby) and a handful of tools built
with them, and every later phase builds on them; `41_system` the ports that reach neither the
graphics stack nor a toolkit; `42_graphics` those that reach the graphics stack but no toolkit;
`43_toolkits` those that reach a toolkit; `44_apps` the applications nothing depends on, which reach
a toolkit or, filed on an application shelf, the graphics stack. Every port's dependencies sit in
its own phase or an earlier one; [Writing ports](../05-developer/writing-ports.md#which-phase-lists-a-port)
gives the rule a new port is placed by. The
split puts a restore point after `30_foundation`, about the first hour of native building, in front
of the compilers that take most of the hours before `40_lang`, and another after each layer of the
userland.

`41_system`, 1,014 ports, is one phase because the graph does not support a finer cut. Any split
of it by subject pushes a hundred or more ports past their subject's phase, because a system
library filed on a hardware shelf sits under `openssh`, `gnutls` and `cups`; a split by dependency
depth is valid but puts `zsh` beside `libpng`. Its `packages.d/` gives it its grouping instead.

**From `30_foundation` on, a phase's list names exactly the ports the phase installs.** A list
that names only what it wants, and lets `depends =` pull in the rest, lets a port move silently
into an earlier phase when something there starts depending on it, and lets a list reach forward
into a later phase's ports. `testing/phaseclosure.py`, run by preflight, requires each list's
closure, less what earlier phases installed, to equal the list, and names each port that breaks
it with both phases.

**A large list is a directory of shelf files.** The five userland phases, `40_lang` to
`44_apps`, each read `packages.d/*.txt` in byte order as one list: one `<shelf>.txt` per shelf the
phase draws on, one `src-<area>.txt` for the ports it builds from an area under `src/`, and, where
a phase needs a run whose order a comment pins, an `00-order.txt` that sorts first and holds it.
`41_system` carries that file with its header and no run in it. A phase with one short list keeps a single `packages.txt`: its pinned run first and,
where the list holds more, the rest grouped by shelf under comment banners. The lists of
`20_selfhost` and `60_kernel` are a pinned run alone.

**Shared settings are written once.** `script/env/common.env` holds what every phase sets (the
reproducibility variables, `MAKEFLAGS`, strict recipe matching), `script/env/host.env` what the two
host phases share, and `script/env/chroot.env` what every chroot phase shares, such as the pinned
compiler. A `phase.env` sources one of them and sets only what differs. The keys the orchestrator
reads, the title, the description, the snapshot paths and exclusions, and `CHROOT`, stay in the
`phase.env` text itself, because the orchestrator reads that file's text and follows no `source`: set
in a shared file instead, `CHROOT=1` would run every chroot phase on the host. `PORT_REPO` stays there
too, because `testing/phaseclosure.py` reads it from that text in the same way; a phase that sets none
searches kpkg's default, `/ports/core`.

The pre-push hook sits in `script/hooks/` and nowhere else, because a clone is set up once with
`git config core.hooksPath script/hooks`, and a hook at any other path would silently not run; see
[The pre-push check](../05-developer/developing.md#the-pre-push-check).

The cost is more phases and more snapshots. Snapshots are off by default; a build asked for them
(`--snapshot`, or `S` in the start-up picker) saves, as each phase up to `50_desktop` finishes, a
compressed layer of what that phase changed in the tree, on top of the whole tree the first phase
saved, so each restore point is paid for in disk. A phase opts out by leaving
`KDOS_SNAPSHOT_PATHS` empty, as `60_kernel` and `70_image` do. See
[Snapshots](../05-developer/build-system.md#snapshots). A list that names every port it installs is also longer
than one that names only what it wants, and a new dependency that crosses a phase boundary fails
preflight until a list moves. Some real edges in the graph become visible this way: `podman`,
`distrobox`, `qemu`, `libvirt` and `sane-backends` build in `43_toolkits`, because `gpgme` reaches
Qt through `gnupg` and `pinentry`, and `kdos-tools` builds in `42_graphics`, because `kbd` reaches
Wayland through `libxkbcommon`. See [The build system](../05-developer/build-system.md#phases).

## Signing the index, not every package

A [binhost](../06-reference/glossary.md#binhost) (a directory of prebuilt host packages with a signed
index) is protected by one Ed25519 signature over its index, `PACKAGES.sig` beside `PACKAGES`.
The index carries every package's SHA-256, so one signature covers all of them transitively. A
per-package `<file>.sig` sidecar exists for the separate case of a package travelling on a USB
stick with no index beside it.

Signing every artefact as the primary mechanism was rejected because it multiplies the work and
the number of things that can be individually wrong without improving what is proven.

A signature file holds any number of signatures, one per line, and verifies when at least one line
verifies against a key in the local keyring. During a key rollover both keys sign and a client
trusting either keeps working. Adding that to a format that holds only one signature would be a
format change every client has to understand. The key id inside a signature is a label, not a
selector: every line is tried against every key in the local keyring and against nothing outside
it, so the id can never supply a key. See [Packaging](../03-architecture/packaging.md#signing).

## Updates are signed slot images, with a binhost on the way

An update of the operating system is a release, written into the root slot that is not running and
booted on trial. Each release publishes, signed with its release key, what its build produced: the
packages on a binhost with deltas against the release before, and whole-slot images with deltas
from each of the two releases before it. `kdos update apply`, and Discover through
`kdos-sysupdated`, which answers Discover's `sysupdate1` calls, do all the downloading and writing
in one step while the running system is untouched, then ask for a restart. Nothing is applied
unasked, and the nightly check reads only what is already on the machine. A release that carries
an image updates by image: the image is the base, and the world set follows it into the slot from
the same release's binhost. Otherwise the inactive slot is filled from the binhost, and a machine
with a package the binhost does not match is told so before the slot is touched. Either way a
package that was built on the machine is built again in the slot, and the rest are downloaded.
`kdos update apply --source` builds the release's signed recipe tree on the machine instead.

The new slot is kept only once a boot of it has finished `rcS` and then had a Plasma session (or,
on a machine where nobody logs in, the login screen) up and answering for a minute; `kdos-sysupdated`
marks it good only when both have happened. A trial that never gets that far uses up its attempts
and the next boot falls back to the slot that worked.

Building every update on the machine was the alternative, and it needs no published binaries at
all: one signed recipe tree is the whole trust root, and every source is checked against a hash it
names. It was not taken as the default because an update that compiles a new Qt or a browser takes
hours and the network for every source, a machine offline cannot update, and a person who asked for
a desktop should not wait for a compiler. Images are preferred to packages because an image is one
hash for the whole root: it cannot half-apply, and the slot it writes is the slot that was tested.

The costs: a release has to build, sign and publish its binhost and images beside the ISO; the
release key vouches for binaries, not only for a recipe tree; and every update ends in a restart.
Every binary is still built from this tree's recipes, so a published package can be rebuilt and
compared byte for byte ([Principles](principles.md#reproducible-by-construction)). See
[Packaging](../03-architecture/packaging.md#updating-a-machine).

## One installer backend, two front ends

The installer is one backend, `kinstall`'s probing, configuration and installation code, under two
front ends: a Kirigami application on the live desktop, and a text interface for a serial console
and for unattended installs. The graphical front end asks the same questions the text one does,
runs as the live session's user, asks the backend what the machine holds and what an answer would
do, starts it as root through polkit with the answers on its standard input, and follows its
progress as a stream of JSON events, so no Qt code runs as root and the two front ends share one
check of the answers.
A manual disk layout is made in KDE Partition Manager, and the installer takes it from there. At the
account step the installer either creates the user, which is the default, or leaves the machine for
its first owner, who meets plasma-setup at the first boot.

Two installers were the alternative: a graphical one written in Qt beside the text one kept for
serial consoles. It was not taken because two installers are two implementations of what a disk
layout, an encrypted root and a pair of root slots mean, and the one nobody is testing drifts
([Principles](principles.md#one-implementation-of-an-idea)). The backend links no toolkit, so it
still runs from a serial console and before any desktop exists. The cost is an event protocol
between the backend and the graphical front end that both sides have to keep.

## Freezing a demo rather than writing one

The ASCII-art demo, `kdos-bb`, is a frozen hard fork of the AA-project's `bb` 1.3rc1, recorded in
`src/art/kdos-bb/KDOS-FORK`. Of the 81 files upstream ships, it keeps the sources and headers
the binary is built from, the three `.s3m` tracker modules, and the authors' own credits scroll; the
autotools apparatus is replaced by a static `aconfig.h` and a `build.sh` that calls the compiler.
The fork's `src/` holds 44 C sources, one of them `kdostux.c`, an image generated by `genimg.py`,
and 16 headers, one of them `aconfig.h`.

`KDOS-FORK` lists every change the fork makes, and they fall into two groups.

Some changes are needed for the demo to run here. `clear_zbuff()` clears exactly the `sizeof(int)`
per cell it allocates; clearing `sizeof(long)`, double that on x86-64, corrupts the heap. The
message scroll moves overlapping ranges with `memmove`, because `memcpy` on them is undefined and
musl does not tolerate it. `REGISTERS(n)` expands to nothing, because the x86-32-only `regparm`
attribute warns on every declaration here. The drawing is paced on a deadline grid of one frame per
16 ms, the mixer runs on its own thread so the music does not starve while a frame is drawn, and
the scene clock follows the music player so the two stay in step. Each frame is wrapped in
synchronized output (DECSET 2026), so a terminal that supports it draws whole frames rather than
halves of two.

The rest change what the demo shows and how it starts. It starts by itself, with `-nosound` and
`-mixer` as flags in place of upstream's two start-up questions; the flash words and the closing
text carry KDOS's mark, and the mascot fills two beats (the flash word upstream left as an empty
strobe, and a logo beat before the closing text scrolls); and the closing text turns its own pages
in time with the last module. Every upstream contributor line and every Special Thanks in the
credits is kept unchanged; the credits add one KDOS entry, for the musl fix and the threaded mixer,
and point to `KDOS-FORK` beside `COPYING` for what the fork changed.

A demo written from scratch was the alternative. `bb` is a set of scenes paced against the three
tracker modules it ships with, and reaching that from nothing would be a project of its own, where
the fork needed only the changes listed above. See [kdos-bb](../04-programs/kdos-bb.md).

## Narrowings

A narrowing is a decision that reads like a gap. The thing is not there, and it is not there because
a smaller answer was chosen over a larger one. Each of these is too small for a full entry of its
own, and each has the same shape: what was asked for, what is built instead, and what the larger
answer would have cost.

### Invitations are read, not answered

An invitation in a message `aerc` shows is read and never answered. The shipped `aerc`
configuration sends `text/calendar` parts through `kdos-part ics`, which runs aerc's own calendar
filter: it prints the event — summary, times, location, who was asked — and writes nothing
anywhere. A filter runs every time a message scrolls past, so one that imported would accept every
meeting it was scrolled over. Filing an invitation from aerc is manual and works: `:save` the part
out of the message and `khal import` it, or open it, which hands it to the system's calendar
handler, Thunderbird.

### No raw-block-device applications

Applications that need raw block devices are not in the catalogue: partitioners, drive-health
tools, recovery tools. A rootless container cannot do anything useful with a block device, and a
launcher that opens onto a permission error teaches somebody that the machine is broken. Those jobs
are native tools on the host, where the privilege is: `parted`, `gptfdisk`, `smartmontools`,
`testdisk` and `ddrescue` are ports.

### No other compositor's private protocols

Programs that need another compositor's private protocols are not on the host. `grim` and
`wf-recorder` capture the screen through the wlroots capture protocols, which KWin does not
implement, so screenshots and recordings are Spectacle's, which asks KWin's own screenshot
interface and the screen-cast portal, and `kdos-shot` is a front end to it. The desktop is shared
over VNC by krfb and over RDP by krdp, which reach the screen the same way.

### Nothing downloads at run time

Nothing downloads a component at run time. `winetricks` is not in the catalogue, because it
fetches Windows runtimes from the network when it runs, and for the same reason Wine gets no
Microsoft core fonts: a Windows program that wants a specific proprietary font gets a substitute.
KDE's "Get New…" service, KNewStuff, is switched off for every program (`ghns=false` under
`[KDE Action Restrictions]` in the distribution's `kdeglobals`), so no settings page offers to
fetch a theme, a wallpaper or a script from store.kde.org; what KDOS offers instead is a curated
add-on pack in the base, which Discover lists. Once installed, nothing may depend on a download
an offline machine cannot make.

### A library edit rebuilds every in-tree port

Editing a library rebuilds every port of KDOS's own software that builds from the tree, not only its
consumers. A port under `src/` with no `source =` line builds from the tree, as every port there
does, and each one that uses a `libk*` library compiles it into itself, its `build.sh` naming the
ones it uses as a glob under `$LIBS`. Working out which ports an edit reaches would need a shell
parser inside the package manager, so `kp_hash.c` puts the whole of `src/libs` into the recipe hash
of every such port instead. The test is the recipe and where it sits: the hash walks `src/libs` for
every port with no source (no `source =` line, or an empty one) whose directory has a `../../libs`
beside it, which is every port under `src/`, since each sits at `src/<area>/<name>/`. The two
source-less ports under `ports/core`, `musl-ldd` and `containers-common`, sit on a shelf, where
`../../libs` is `ports/core/libs`, a name no shelf may take, and hash their own directory alone, and
every other port under `ports/core` is unaffected. Each rebuild takes seconds, while a parser would
be a second build system whose mistakes show only as missed rebuilds.

### util-linux switch_root in the initramfs

The initramfs carries util-linux's `switch_root`, which moves the new root onto the root of the
mount namespace before it `chroot()`s. A `switch_root` that only `chroot()`s leaves every process
on the booted system chrooted. The kernel refuses a chrooted caller a new user namespace, so every
rootless container would be refused, and the machine boots normally either way. The initramfs step
refuses to build an initramfs whose `switch_root`, `mount`, `umount`, `losetup`, `dmesg`, `setsid`
or `blkid` does not name util-linux in its `--version`. See
[Boot and init](../03-architecture/boot-and-init.md#switch_root).

## See also

- [Why KDOS](why-kdos.md) — the properties these decisions serve
- [How KDOS differs](how-kdos-differs.md) — how other distributions answer the same questions
- [Principles](principles.md) — the rules that follow from them
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the pack and store decisions as built
- [Session](../03-architecture/session.md) — the login, elogind, turnstile and Plasma's start-up
  as built
- [The window model](../03-architecture/window-model.md) — KWin as KDOS configures it
- [Writing ports](../05-developer/writing-ports.md) — the recipe format and publishing sources
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the build from a clone to an ISO,
  where the source archive and the recipe hash come into play
- [The ports catalogue](../06-reference/ports-catalogue.md) — every port the build and security
  decisions apply to, by shelf
- [Known gaps](../06-reference/known-gaps.md) — what these decisions leave undone
- [Glossary](../06-reference/glossary.md) — the terms these entries use

<!-- book-nav -->
---

*Part I — Introduction, chapter 4.* Previous: [3. Principles](principles.md) · [Contents](../README.md) · Next: [5. Getting started](../02-user-guide/getting-started.md)
