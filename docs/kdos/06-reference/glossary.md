# Glossary

This chapter defines the vocabulary the rest of the book uses, one entry per term, in alphabetical
order. Each entry is a heading, so another chapter can link one term directly
(`glossary.md#root-slot`). It is for any reader who meets a word in another chapter and wants its
exact meaning; it assumes nothing beyond general familiarity with Linux. Where a term has a general
meaning elsewhere and a narrower one in KDOS, the entry gives the KDOS meaning, which is the one
these pages use. Each entry links the chapter that treats the term in depth. A word printed in
*italics* inside an entry has an entry of its own. No other chapter is needed first, though
[Architecture overview](../03-architecture/overview.md) and [How KDOS is
built](../05-developer/how-kdos-is-built.md) give most of these terms their context, and [How KDOS
differs](../01-philosophy/how-kdos-differs.md) sets the host's components (*musl*, *sysvinit*,
*elogind*, *Plasma*) beside the ones other distributions use.

The section after the terms lists words the book avoids, and what it says instead.

## Terms

### accent

One of the eight palettes: phosphor, amber, ice, bone, norton, borland, perfect and paper, defined
in `src/libs/libkcolor/kcolor.h`. Bone is the default; paper is the one light accent. Every accent is
generated at build time into a KDE colour scheme, a *global theme*, a Konsole profile, a qt5ct
palette, a wallpaper and the login screen's look, so changing it with `kdos theme <accent>`, or on
System Settings' KDOS Look page, chooses among files that already exist. The accent in force is one
word in `~/.config/kdos/accent`; the boot menu, the splash and the console read the machine's copy
in `/etc/kdos/accent`. See [Theming](../02-user-guide/theming.md).

### alien app

A graphical application that is not compiled by this repository. It is built on your machine from a
*catalogue* row, or imported as *packs*, and runs in a *box*. The opposite is a *native
application*. See
[Applications](../02-user-guide/applications.md).

### appbox

The program that installs, launches and exports boxed applications, `kdos-appbox`. Used loosely, the
word means the whole mechanism by which alien apps run. See
[kdos-appbox](../04-programs/kdos-appbox.md).

### area

One of the six directories under `src/` that divide KDOS's own code by what each program is:
`desktop` (KDOS's Qt and QML parts of the Plasma desktop: settings pages, widgets, KWin effects and
scripts, the Discover backend, the graphical installer), `daemons` (root daemons the desktop
account talks to), `system` (the package manager, the `kdos` command, packs, boxes and the
installer), `art` (the identity generators, icons, wallpapers, sounds, the login theme and the
splash), `libs` (the `libk*` libraries) and `devtools` (build tools that are never installed). Every
port of KDOS's own sits exactly at `src/<area>/<name>/`, so that `../../libs` from it is `src/libs`.
The four areas that hold recipes are *port* repositories; `libs` and `devtools` hold none. An area
is not a *shelf*. See [Packaging](../03-architecture/packaging.md#the-recipes-under-src) and
[Decisions](../01-philosophy/decisions.md#kdoss-own-code-is-divided-by-what-each-program-is).

### bake

The image build, and in particular its last phase, `70_image`, which turns the *target tree* into
the ISO. Something **baked into the image** is put there at build time rather than installed on the
running machine. See [How KDOS is
built](../05-developer/how-kdos-is-built.md#packaging-70_image).

### base

The bottom of an application's image chain: a whole root filesystem rather than a difference over
another. The catalogue names two, `alpine` and `base` (Debian). A **base pack** is a base carried as
a *pack*; building with `KDOS_PACK_KDOS=1` also packs the KDOS root filesystem itself as the base
pack `kdos`. See [Packs and boxes](../03-architecture/packs-and-boxes.md#the-catalogue).

### binhost

The **binary host**: a directory of prebuilt host packages with a signed index. `kpkg binhost`
installs from one on a local path, such as a USB stick or an NFS mount, and `kdos update` takes one
from a release's `binhost-<version>-NN` releases, or from `binhost =` in `/etc/kdos/update.conf`,
each file found through the release's signed *release index*, `kdos-update.idx`. A package from
it is used only when its architecture, its *build-config hash* and its *recipe hash* all equal this
machine's, and once installed it is recorded as that recipe's build, so the next build does not
rebuild it. See [Packaging](../03-architecture/packaging.md#the-binary-host).

### bless

The confirmation that keeps a *trial boot*: `kdos-bootctl mark-good`, which `kdos-sysupdated` runs
once both marks under `/run/kdos/bless/` exist, `userspace` from the end of *rcS* and `graphical`
from a Plasma session that stayed up for a minute. Either mark alone confirms nothing, so a slot
whose services start and whose desktop crashes is rolled back. See
[Discover](../04-programs/discover.md#restart-trial-and-confirmation).

### box

A rootless podman container that one application, or one working environment, runs in. A **store
box** is a distrobox container created from the image the store *lane* built; a **pack box** is
created over an overlay of mounted *packs* plus a writable layer. A box's windows reach KWin through
a socket tagged with *security-context-v1*, so KWin refuses them the desktop-wide Wayland globals a
box's profile does not grant, and a box is given a private runtime directory and a filtered session
bus. It is not a boundary against you: it shares your home directory. See [Packs and
boxes](../03-architecture/packs-and-boxes.md#the-box) and [The security
model](../03-architecture/security-model.md).

### box colour

The colour a box's windows wear in their title bars, chosen with `accent =` in its *box profile* or
on System Settings' Boxes page. `kdos-appbox` writes one KWin window rule per box that forces the
title bar to the colour scheme of that accent, so a box's colour marks its security domain; a box
with none wears the neutral `KDOSBox` scheme, a grey title bar lighter than any accent's. See
[KWin](../04-programs/kwin.md#the-box-colour).

### box profile

`~/.config/kdos/boxes/<name>.conf`, a handful of settings for one box. Most keys map onto a
container-engine flag or onto something KDOS enforces itself; a few, such as `persistence` and
`export`, are only recorded. `kdos-box profile <name>` prints each key beside the mechanism that
applies it, and marks with `!` a setting it cannot enforce. See
[kdos-appbox](../04-programs/kdos-appbox.md#box-profiles) and
[Configuration](configuration.md#configkdosboxesnameconf).

### build-config hash

One of the two hashes that decide whether a prebuilt package matches this machine: a SHA-256 over
the architecture, the target, the C library, the compiler and its version, and the compiler flags.
Written `B:` in a package index. See also *recipe hash*, and
[Packaging](../03-architecture/packaging.md#b--the-build-config-hash).

### catalogue

The shipped list of every application KDOS knows how to build, as Debian packages:
`src/system/kdos-appbox/catalogue`, installed as `/usr/share/kdos/appstore/catalogue`. Its rows
are *bases*, *runtimes*, applications, data sets and groups of applications. Discover lists it
through the KDOS backend. Not to be confused with [the ports catalogue](ports-catalogue.md), the
chapter that lists every host *port*. See [Packs and
boxes](../03-architecture/packs-and-boxes.md#the-catalogue).

### chroot

A process whose root directory has been changed, so that it sees one directory tree as `/`. Every
build phase from `20_selfhost` onwards runs each of its commands inside the *target tree* `build/fs`
through `script/chroot/exec.sh`, with a cleared environment to which it adds only the variables it
names, so a build uses only the compilers, libraries and tools that earlier phases put there and
nothing from the build container. `script/chroot/enter.sh` is the interactive counterpart, for
inspecting the tree by hand; the build does not use it. See [The build
system](../05-developer/build-system.md#the-chroot).

### commit

In Wayland, the request by which a client makes the state it has attached to a surface, its buffer
among it, take effect; nothing a client changes on a surface is shown until it commits. See
*configure*, and [KWin](../04-programs/kwin.md).

### compose

To build a pack box's root filesystem as an overlay of its pack stack, which `kdos-packd` does on
request. Composing is safe to repeat, because mounts are reference counted, and it is redone before
every start, because the overlay lives on a temporary filesystem that a reboot empties. See [Packs
and boxes](../03-architecture/packs-and-boxes.md#composition).

### configure

In Wayland, the compositor's message that gives a surface its size and state. The client
acknowledges it, then draws and *commits*. The acknowledgement quotes the configure's **serial**,
the number the compositor attaches to a configure or an input event so that a later request can show
which event it answers. See [KWin](../04-programs/kwin.md).

### cross toolchain

A compiler and linker that run on one system and produce programs for another. The first build
phase, `00_cross`, builds one into `build/cross` for the target `x86_64-kdos-linux-musl`, a system
that does not exist yet; `10_bootstrap` uses it to compile the C library and a minimal userland into
the *target tree*. See [How KDOS is
built](../05-developer/how-kdos-is-built.md#the-cross-toolchain-00_cross).

### data pack

A *pack* carrying a dataset rather than a program. It is mounted read-only with execution disabled
and never composed into a box root; it can reach its consumers only through *grafts*. See [Packs and
boxes](../03-architecture/packs-and-boxes.md#grafts-and-data-packs).

### delta

A binary difference between two packages, two packs or two *slot images*, made with `zstd
--patch-from`. A package delta is taken over the uncompressed tars; a pack delta is taken over the
two packs as they are, which works because *EROFS* compresses each cluster separately; a slot image
delta is taken file by file, against the file the same path held in the earlier version. A delta
carries no signature of its own: what it rebuilds is checked against a hash a signed index or
manifest already carries. See [Packaging](../03-architecture/packaging.md#deltas) and [Packs and
boxes](../03-architecture/packs-and-boxes.md#deltas).

### desk

One of the three panel layouts KDOS offers, each a Plasma *layout template*: **KDOS** (the default:
one opaque panel along the bottom of each screen, with the Start menu, the virtual desktops, the
pinned applications, the meters, the tray and the clock), **Dock** and **Tiling**, which also
turns auto-tiling on. The Welcome Center asks which one at the first login,
and `kdos desk <name>` changes it later, keeping a copy of the panels it replaces so `kdos desk
undo` can put them back. See [The Plasma shell](../04-programs/plasma-shell.md#the-desks).

### Discover

KDE's software centre, `plasma-discover`. On KDOS it shows the *catalogue*, *packs*, *boxes* and
host *ports* through the KDOS backend, firmware through fwupd, and the system update through
*sysupdate1*; it has no Flatpak and no add-on downloads. See [Discover](../04-programs/discover.md).

### elogind

The login and seat manager taken out of systemd, run on its own: it serves `org.freedesktop.login1`,
tracks sessions, hands the display and input devices to the session in front, and carries out
suspend, power-off and reboot on polkit's say. SDDM, KWin, PowerDevil and polkit all ask it. It is
started by `/etc/init.d/40_elogind.sh` and supervises nothing. See [The
session](../03-architecture/session.md) and
[Principles](../01-philosophy/principles.md#no-systemd).

### EROFS

The Linux kernel's Enhanced Read-Only File System, a compressed filesystem that can only be read.
Every *pack* is an EROFS image, compressed with zstd, with its metadata and signatures appended
after the image. See [Packs and
boxes](../03-architecture/packs-and-boxes.md#a-pack-is-an-image-with-parts-appended).

### ESP

The EFI system partition, the partition UEFI firmware loads its boot program from. On an installed
disk it holds the Limine loader, the boot menu `limine.conf`, one kernel per *root slot* and the
boot state under `EFI/kdos/`, which `kdos-bootctl` owns. See [Boot and
init](../03-architecture/boot-and-init.md#one-kernel-per-slot).

### exclusive zone

The strip of an output that a *layer surface* reserves, so that windows are laid out clear of it.
A Plasma panel that is always shown has one. See [KWin](../04-programs/kwin.md).

### fixture

Recorded system state that a program can be pointed at instead of the live machine, which is what
makes its readings and decisions testable. There are 30 fixture directories under
`testing/fixtures/`. See [Testing](../05-developer/testing.md#fixtures).

### global theme

A Plasma look-and-feel package: one directory under `/usr/share/plasma/look-and-feel/` that names a
colour scheme, a Plasma style, a window decoration, icons, a cursor, a wallpaper and a splash screen,
applied together. KDOS ships one per *accent*, `org.kdos.<accent>.desktop`; System Settings' Global
Theme page and `kdos theme` apply them. See [Theming](../02-user-guide/theming.md).

### glyph tier

Which set of drawing characters a terminal program built on `libktui` may use, chosen from the
terminal's capabilities. There are three: **rich** (UTF-8 outside the Linux console, with
eighth-block bars), **vt** (UTF-8 on the Linux console, with three-level bars) and **ascii** (no
UTF-8). The vt tier exists because the console font has 512 glyphs, and a character it lacks is
drawn as a blank. `kinstall`'s terminal wizard is the program that meets all three. See [The design
language](../03-architecture/design-language.md#the-glyph-tiers).

### graft

A declared placement of a *data pack*'s contents where a consumer will look for them. There are two
namespaces, because the host's data directories are invisible inside a box. Each graft is recorded
in a manifest, so that removal is exact. `kdos-packd` places a graft when asked `graft <id>`, which
`kdos-appbox import` asks for every data pack it installs. See [Packs and
boxes](../03-architecture/packs-and-boxes.md#grafts-and-data-packs).

### greeter

The login screen: the program that asks who you are before a session exists. On KDOS it is SDDM's
greeter in the `kdos` theme, running on its own copy of KWin on Wayland, as the `sddm` account. See
*SDDM*, and [Boot and init](../03-architecture/boot-and-init.md).

### group

A word with two unrelated meanings, which the book keeps apart.

- The **`group =` key** in a recipe is read only by the upstream version checker, `ports/update`,
  which offers the members of one group as a single bump, and only when every member has the same
  newer version available. Without the key, the checker derives a group from the source URL's
  organisation on GitHub, Codeberg, sr.ht or a GitLab instance, together with the version. The
  recipes that set the key are pairs built from one upstream release, such as `glib` and
  `glib-introspection` (`group = glib`) or `gcc-arm-none-eabi` and `libstdcxx-arm-none-eabi`
  (`group = gcc-arm-none-eabi`), and the Qt modules (`group = qt6`, `group = qt5`), which
  `download.qt.io` releases together; [The ports catalogue](ports-catalogue.md#how-the-catalogue-is-organised)
  lists them all. The members of one group are filed on one *shelf*. See
  [Packaging](../03-architecture/packaging.md#the-group-key).
- A *catalogue* row of type `group` is a set of applications installed together.

A *shelf* is a third thing, distinct from both: the subject directory a port is filed under, by
which the *package lists* are also grouped.

### held snapshot

A *snapshot* that was replaced, or deleted with `--delete` or the picker's `D`, while another
snapshot's chain of *snapshot layers* still runs through it. It moves to
`build/snapshots/.held/<phase>@<id>/`, so every snapshot built on it still restores, and is deleted
as soon as no phase's snapshot needs it. `make snapshots` lists it with what needs it. See
[The build system](../05-developer/build-system.md#what-a-snapshot-directory-holds).

### host

The KDOS system itself: everything compiled from this repository, as opposed to what runs in a box.
In a build context, also the machine you are building on. See [Architecture
overview](../03-architecture/overview.md#the-host-and-a-box).

### initramfs

The small archive the kernel unpacks into memory and runs first. Its `init`, a bash script, finds,
unlocks and mounts the real root (or, on the live *medium*, builds an *overlay* root), then hands
over to it with `switch_root`. Packaging generates it as `build/initramfs.cpio.gz`, and it carries
GNU coreutils, grep and sed, util-linux's mount tools, bash, a module set and the programs early boot needs. See [Boot and
init](../03-architecture/boot-and-init.md#the-initramfs).

### KCM

A KDE configuration module: one page of System Settings, built as a plugin that System Settings, or
`kcmshell6`, loads. KDOS's own are Boxes, Firewall, Health, KDOS Look and System Versions. See
[KDOS's settings pages](../04-programs/kdos-settings.md).

### KConfig

The KDE Frameworks library behind every KDE program's settings, and the INI-style file format it
reads: `[Group]` headers and `Key=value` lines, merged from every directory on `XDG_CONFIG_HOME` and
`XDG_CONFIG_DIRS`, the first directory winning key by key. `kwriteconfig6` and `kreadconfig6` change
and read one key. See [Configuration](configuration.md#where-configuration-lives).

### kded

`kded6`, the KDE daemon that hosts small session modules in one process: the screen and keyboard
configuration, the device notifier, KDOS's own `kded-kdos`, which keeps the *accent* in step with the
global theme chosen in System Settings. See [The Plasma shell](../04-programs/plasma-shell.md#kded-kdos).

### kdos-packd

The root daemon that is the only program on the system to mount a *pack*. It verifies and installs
packs, mounts them, *composes* a pack box's root from them and places *grafts*, and answers root and
members of `wheel` only. `kdos-appbox` is its client. See [The
daemons](../04-programs/daemons.md#kdos-packd).

### key chord

A key combination pressed together, such as `Meta+Return`. KDE's global shortcuts are kept by
`kglobalacceld` in `~/.config/kglobalshortcutsrc`; KDOS adds its own chords at the first login from
`/usr/share/kdos/xdg/kglobalshortcutsrc`, and System Settings' Shortcuts page rebinds any of them. A
chord that only opens a place reaches it through a *route*. The key card, `Meta+F1`, lists them all.
See [KWin](../04-programs/kwin.md#shortcuts).

### kimpanel

The D-Bus panel protocol through which an input-method engine such as fcitx5 hands its candidate
list and *preedit* to a separate window. Plasma's Input Method Panel widget, on the KDOS desk's
panel, speaks it. See [The session](../03-architecture/session.md#input-methods).

### KIO

KDE Frameworks' network-transparent file access: a program that opens `sftp:/`, `smb:/` or `trash:/`
through KIO has a *worker* process fetch it. KDOS adds `library:/`, the offline library as a folder.
See [The Plasma shell](../04-programs/plasma-shell.md#the-library-as-a-folder).

### Kirigami

KDE Frameworks' QtQuick toolkit for applications and settings pages that work at any size. KDOS's
own settings pages and its graphical installer are written in it. See [Writing desktop
software](../05-developer/writing-desktop-software.md).

### kpkg

The host package manager. It reads a *port*'s recipe, builds it into a package, installs it into the
package database under `/var/lib/kpkg/db/`, and decides what needs rebuilding from the *recipe
hash*. Its source is `src/system/kdos-kpkg`, which `10_bootstrap` compiles directly because nothing
can be installed as a port until `kpkg` exists. See
[Packaging](../03-architecture/packaging.md#kpkg).

### kpkgbuild

The metadata file of a *port*: `key = value` lines naming the version, the sources and their hashes,
the dependencies and the build settings. It is parsed, never run as shell. See [Writing
ports](../05-developer/writing-ports.md#kpkgbuild).

`kpkgbuild` is also one of the five names of the `kpkg` binary: under that name it builds the port
in the current directory into a package without installing it. See [Command
index](command-index.md#packaging).

### kpkgdepends

The `kpkg` tool that resolves a list of ports, through their `depends =` lines, into the order they
must be installed in, and prints that order as one space-separated line and nothing else, because
the build orchestrator reads its output. See [Packaging](../03-architecture/packaging.md#kpkg) and
[The build system](../05-developer/build-system.md#how-a-phase-runs).

### KRunner

Plasma's search and command line, on `Alt+Space`: it asks each of its runners about what you type
and lists their answers. The KDOS runner adds the `kdos` verbs, the *routes*, the two-letter codes of
`~/.config/kdos/favorites`, packs on the boot medium, shortcuts and the offline library. See [The Plasma
shell](../04-programs/plasma-shell.md#krunner-and-the-kdos-runner).

### ksvc

The program from `kdos-tools` that supervises the root daemons, restarting one five seconds after it
exits and sending its output to syslog. Under the name `service` it lists, starts, stops, enables
and disables the service scripts `/etc/init.d/NN_<name>.sh`, and with `--user` a user's own
services in `/etc/kdos/user-services`, which it runs as *turnstile*'s backend. The same binary,
installed as `/usr/sbin/ksvc`, is also `kdos`, `kdos-getty`, `kdos-bootctl` and other commands, each
a separate program chosen by the name it is started under. See [The
daemons](../04-programs/daemons.md#supervision-and-logging) and [The kdos
command](../04-programs/kdos-command.md#ksvc-and-service).

### KWin

Plasma's compositor and window manager, `kwin_wayland`: it takes the display and the input devices,
places, decorates and draws every window, runs the effects, keeps the global shortcuts and the
lock screen, and starts *Xwayland* when an X11 program connects. KDOS builds it with three patches of
its own, among them the *Phosphor mode* effect. See [KWin](../04-programs/kwin.md) and
[Decisions](../01-philosophy/decisions.md#kwin-is-the-compositor).

### Landlock

The Linux kernel's unprivileged sandboxing interface: a process restricts which directory trees it
and everything it starts may reach, and on later ABI versions which TCP ports and which IPC, with no
root and no container. `kdos sandbox` runs a native program under a Landlock profile through
`libkbase`'s `kb_landlock_*`. See [The kdos command](../04-programs/kdos-command.md#kdos-sandbox)
and [The C libraries](../05-developer/c-libraries.md#landlock).

### lane

One of the two routes an application takes to a box. The **store lane** builds container images from
the catalogue on your machine, over the network; the **import lane** (or pack lane) installs signed
packs someone exported. Both are separate from host packaging. See [Packs and
boxes](../03-architecture/packs-and-boxes.md#two-lanes-one-box).

### layer surface

A surface of the wlr layer-shell protocol (`zwlr_layer_shell_v1`), placed by anchoring it to edges
of an output, on a layer above or below the windows, rather than as a window. Plasma's panels and
desktop are layer surfaces, and so is the greeter. KWin refuses the protocol to a box unless its
profile grants `layer-shell`. The strip one reserves is its *exclusive zone*. See
[KWin](../04-programs/kwin.md#what-a-box-may-bind).

### layout template

A Plasma package of a script that lays out panels and widgets, under
`/usr/share/plasma/layout-templates/`. KDOS's three *desks* are the templates
`org.kdos.desk.kdos`, `org.kdos.desk.dock` and `org.kdos.desk.tiling`. See [The Plasma shell](../04-programs/plasma-shell.md#how-a-desk-is-laid-out).

### march ledger

`/var/lib/kdos/march.ledger`, where `kdos march` records its verdict for each port it has built with
the highest x86-64 feature level the CPU allows and benchmarked against the default build: kept,
reverted or unmeasurable. `kdos march report` prints it. See [The kdos
command](../04-programs/kdos-command.md#kdos-march) and
[Decisions](../01-philosophy/decisions.md#-march-measured-per-machine-not-chosen-for-a-population).

### medium

The USB stick or disc image KDOS boots from. It carries the system and the catalogue; applications
are built on the machine that asks for them, or imported from packs. See [Boot and
init](../03-architecture/boot-and-init.md#the-live-medium-and-persistence).

### musl

The C library the host is compiled against; the build's target is `x86_64-kdos-linux-musl`. An
application in a box uses its base's C library instead, which is glibc in the Debian base. See
[Decisions](../01-philosophy/decisions.md#musl-as-the-host-c-library).

### native application

A graphical application compiled by this repository as a *port* and installed on the *host*, such as
Firefox ESR, LibreOffice, GIMP or Kate. It links the host's toolkits (Qt, KDE Frameworks, GTK,
wxWidgets, FLTK, Tk or SDL), which are ports too. Each toolkit that has a Wayland backend is built
with it as the run-time default and its X11 backend compiled in beside it; Tk has only X11, so a Tk
window is an *Xwayland* client. It runs with no *box*. The opposite is an *alien app*. Most are
built in the `43_toolkits` and `44_apps` phases; see [The ports catalogue](ports-catalogue.md) and
[Decisions](../01-philosophy/decisions.md#native-applications-on-the-medium-a-store-that-builds-the-rest).

### overlay

The Linux overlay filesystem (overlayfs), which presents a writable upper layer over one or more
read-only lower layers as a single tree; a change lands in the upper layer and a deletion is
recorded as a *whiteout*. The live *medium*'s root is an overlay over `system.sfs`, and a pack box's
root is an overlay over its packs. See [Packs and
boxes](../03-architecture/packs-and-boxes.md#composition) and [Boot and
init](../03-architecture/boot-and-init.md#the-live-medium-and-persistence).

### pack

One application, runtime, base or dataset as a single signed file: an EROFS filesystem image with a
metadata blob, an icon, a signature block and a footer appended. The extension is `.kpack`. See
[Packs and boxes](../03-architecture/packs-and-boxes.md).

### pack store

`/var/lib/kdos/packs`, where `kdos-packd` keeps installed *packs*, with the `staging` directory an
import passes through and the `mnt` directory packs are mounted under. On an installed machine it
is `/home/.kdos/packs`, bound there, so both *root slots* share it. Not to be confused with the store
*lane*. See [Packs and boxes](../03-architecture/packs-and-boxes.md#installing-a-pack).

### package list

The list of ports a *phase* installs: one port name per line, with `#` comments. A phase keeps it
either in one file, `packages.txt`, or in a directory, `packages.d/`, whose `*.txt` files are read
in byte order as one list; never both. Ten phases have one, from `20_selfhost` to `60_kernel`. The
five userland phases, `40_lang` to `44_apps`, use `packages.d/`: one `<shelf>.txt` per *shelf* the
phase draws on, one `src-<area>.txt` for the ports it builds from an *area*, and, in `41_system`,
an `00-order.txt` that sorts first and holds the runs whose order a comment pins. A
`packages.txt` gives its pinned run first and, where it holds more, groups the rest by shelf under
comment banners; the lists of `20_selfhost` and `60_kernel` are a pinned run alone.
From `30_foundation` on, a list names exactly the ports its phase installs and no others, which
`testing/phaseclosure.py` checks. See
[Packaging](../03-architecture/packaging.md#phases-package-lists-and-shelves).

### phase

One stage of the build: a directory under `script/phases/` whose name is a two-digit number, an
underscore and a word, run in sorted name order by the build orchestrator, `kdosbuild`. There are
thirteen, numbered in bands of ten so that one can be added between two others: `00_cross`,
`10_bootstrap`, `20_selfhost`, `30_foundation`, `31_compilers`, `40_lang`, `41_system`,
`42_graphics`, `43_toolkits`, `44_apps`, `50_desktop`, `60_kernel` and `70_image`. `00_cross`,
`10_bootstrap` and `70_image` hold numbered step scripts; the other ten hold a *package list*. Each
has its environment in a `phase.env` in its own directory, which says whether it runs in the
*chroot*, which port repositories it searches beyond the default `/ports/core` and what its
*snapshot* holds, and then sources the settings every phase shares from `script/env/`. See [The
build system](../05-developer/build-system.md#phases).

### Phosphor mode

KDOS's optional retro look, off by default and switched with `kdos phosphor on` or on the KDOS Look
page: KWin's `kdos_crt` effect draws a CRT pass over the whole screen (bleed, vignette, a floor so
black is never quite black, optional scanlines and curvature), windows unfold and fold like a tube
powering on, the accent moves to a CRT one, and the boot splash plays its power-on. The effect needs
hardware OpenGL, so a virtual machine on software rendering cannot turn it on. Not to be confused
with `phosphor`, one of the eight *accents*. See
[Phosphor mode](../04-programs/kwin.md#phosphor-mode).

### Plasma

KDE Plasma 6.8, the desktop: *KWin*, `plasmashell` (the panels, the desktop and the widgets),
*KRunner*, System Settings, *Discover* and the session around them, with KDE Frameworks and KDE
Gear's applications. KDOS builds it unforked and makes it KDOS through settings, artwork and its own
plugins. See [The Plasma shell](../04-programs/plasma-shell.md) and [The
desktop](../02-user-guide/desktop.md).

### plasmoid

A Plasma widget: one thing on a panel or the desktop, such as the Start menu, the task manager, the
clock or a tray icon, written in QML and loaded by `plasmashell`. KDOS adds the privacy lamps and the
first-run checklist. See [The Plasma shell](../04-programs/plasma-shell.md#the-system-tray).

### port

One piece of host software as this repository describes it: a directory holding a *kpkgbuild* and a
`build.sh` (the build, run by bash with the unpacked source as its working directory). There are
five port repositories in one format, holding 2,431 recipes: the upstream ports in
`ports/core/`, each on a *shelf*, and KDOS's own in four *areas*: `src/system/`, `src/art/`,
`src/desktop/` and `src/daemons/` ([The ports catalogue](ports-catalogue.md#ports-per-shelf) counts
them). (`src/system/kdos-kpkg` has no recipe; `10_bootstrap` compiles it by script.) A port is known by its bare name, which is unique
across all five. See [Writing ports](../05-developer/writing-ports.md) and [The ports
catalogue](ports-catalogue.md).

### portal

How a sandboxed application asks the host to do something it cannot do itself, such as open a file
chooser, share the screen or open a link. The front-end daemon `xdg-desktop-portal` owns the public
D-Bus interfaces, and a backend implements them; on KDOS the backend is
`xdg-desktop-portal-kde`. See [The session](../03-architecture/session.md#portals).

### preedit

The text an input method is still composing, shown before it is committed to the application. See
*kimpanel*.

### preflight

`testing/preflight.sh`, the check that the tree's wiring is consistent (every package resolves,
every recipe parses, every script is valid) without building anything. See
[Testing](../05-developer/testing.md#preflightsh).

### PSI

Pressure stall information: the kernel's measure, under `/proc/pressure/`, of the time tasks spend
stalled waiting for CPU, memory or I/O. `kdos-oomd` acts on a memory-pressure trigger, and System
Monitor shows the readings. See [The daemons](../04-programs/daemons.md#kdos-oomd).

### quantum

The number of audio frames PipeWire processes in one cycle, which sets the audio latency. In a
virtual machine only, `/etc/pipewire/pipewire.conf.d/99-kdos-vm.conf` raises it to 4096 with a
ceiling of 8192. [kdos-bb](../04-programs/kdos-bb.md) uses the word for one mixer buffer. See [The
session](../03-architecture/session.md#the-quantum-and-why-a-virtual-machine-gets-a-bigger-one).

### RAPL

Running Average Power Limit, the processor's energy counters. Only root can read them, so
`kdos-energyd` reads them, and `kdos-energy` and System Monitor's KDOS sensors ask it. The figures
are shares, never watt-hours, because RAPL cannot see the panel, the radio or the disk. See [The
daemons](../04-programs/daemons.md#kdos-energyd).

### rcK

`/etc/init.d/rcK`, the shutdown counterpart of *rcS*: the first runlevel 0 and 6 entry in
`/etc/inittab`, which stops the service scripts in reverse numeric order. See [Boot and
init](../03-architecture/boot-and-init.md#shutdown).

### rcS

`/etc/init.d/rcS`, the script sysvinit's `init` runs once at boot as its `sysinit` entry. It mounts
the filesystems, runs each enabled service script `/etc/init.d/NN_<name>.sh` in numeric order, and
writes the `userspace` half of a trial boot's *bless*. See [Boot and
init](../03-architecture/boot-and-init.md#rcs-and-the-service-scripts).

### recipe

The files that describe how to build one *port*: its *kpkgbuild*, its `build.sh`, and any
`postinstall.sh` and patches beside them. The *recipe hash* is taken over them and every other
file beside them that no `sha256 =` line names, except the book note
[`kfs.md`](../05-developer/writing-ports.md#the-ports-note-kfsmd). See [Writing
ports](../05-developer/writing-ports.md).

### recipe hash

The other of the two package hashes: a SHA-256 over a port's recipe files (`kpkgbuild`, `build.sh`,
`postinstall.sh` and every patch) and every other file in its directory that no `sha256 =` line
names except the book note `kfs.md`, so editing a kernel config beside a recipe changes it. For a port with no `source =` that is
the port's whole directory, and for one of KDOS's own ports under `src/` all of `src/libs` as well, so editing a
program's source or a shared library changes it. Written `E:` in a package index, where only the
file of the version and release the recipe names carries it and every other file of the port has an
empty `E:` that matches nothing. `kpkg binhost` records the `E:` it matched as the installed
package's recipe hash. It decides what the build rebuilds. Not to be confused with a *source hash*. See
[Packaging](../03-architecture/packaging.md#e--the-recipe-hash).

### release index

`kdos-update.idx`, signed, beside a release's ports tree or in a *binhost* or *slot image*
directory: one line per update file, naming the path a client asks for (`binhost/PACKAGES`,
`image/kdos-image.manifest`), the GitHub release and flat asset name that hold it, and its size and
SHA-256. `kdos update` and `kdos image unpack` fetch nothing it does not name and keep nothing
that does not match it. See [Packaging](../03-architecture/packaging.md#the-release-index).

### restamp

Two operations share the word. `kdos-pack restamp` raises a *pack*'s footer to the current format
and takes its digest again over that format's span, dropping the signature block, so the pack must
then be signed and indexed again; it is the repair for a pack that fails its digest check.
`kdos-bootctl theme <accent>` restamps an installed machine's boot menu and console palette in
place. See [Packs and boxes](../03-architecture/packs-and-boxes.md#rules-the-format-keeps) and [Boot
and init](../03-architecture/boot-and-init.md#restamping-an-installed-machine).

### rig

The virtual-machine harness that boots a real KDOS image in QEMU, drives it with keys, pointer and
commands, and photographs the screen: `testing/vnc-shot.py` and the scripts around it. See
[Testing](../05-developer/testing.md#the-qemu-rig).

### ring

Which of three divisions of the software a program belongs to: core (`ports/core/`, Plasma among
it), desktop (the *areas* under `src/` other than `devtools`, whose build-machine tools belong to
no ring) or outer (the *catalogue*). A *shelf* subdivides the core ring; it is not a ring. Build
cost and ownership decide the ring. See [Architecture
overview](../03-architecture/overview.md#the-three-rings).

### root slot

One of the two root filesystems, A and B, on an installed disk. One is live; an update is written
into the other and tried on the next boot, and `kdos-bootctl` selects and confirms slots. See [Boot
and init](../03-architecture/boot-and-init.md#ab-slot-selection).

### route

A stable name for a place in the system, written `verb.noun` (`setup.network`), defined in
`/etc/kdos/menu.conf` or `~/.config/kdos/menu.conf`, opened with `kdos menu summon <route>` and
offered by the KDOS runner in *KRunner*. It keeps working when the chord is rebound or the settings
page moves. See [Configuration](configuration.md#etckdosmenuconf-merged-under-configkdosmenuconf).

### runlevel

The state *sysvinit*'s `init` holds the machine in. `/etc/inittab` lists each entry's runlevels:
`3` is the running system with its consoles, `S` is single-user mode (`sulogin` on the console,
asking for root's password), `0` powers off and `6` reboots. `telinit` changes it and `runlevel`
prints the previous and current one. See [Boot and
init](../03-architecture/boot-and-init.md#rcs-and-the-service-scripts).

### runtime

A layer shared by many applications, holding a toolkit or a family of libraries, such as `rt-gtk` or
`rt-qt`. The catalogue names seven. An application row names a runtime or a base as its parent. See
[Packs and boxes](../03-architecture/packs-and-boxes.md#the-catalogue).

### SDDM

The display manager: the program `kdos-dm` runs on `tty1` from `/etc/inittab`, which shows the
*greeter*, logs a person in through PAM, starts their Plasma session through
`kdos-session-wrapper`, and logs the live medium's account in by itself. See [Boot and
init](../03-architecture/boot-and-init.md) and
[Configuration](configuration.md#etcsddmconfd).

### security-context-v1

The Wayland protocol through which a process hands the compositor a listening socket to tag. Every
client that connects on it carries the tag (an engine name, an application id and an instance id),
which the client can neither see nor change. `kdos-boxsock` creates one socket per box launcher,
tagged with the engine `io.kdos.appbox`, the launcher's id and the box's name; KWin's filter for
sandboxed clients and its grants, the window's application id, the *box colour* and the sandbox
badge all read that tag. See [The daemons](../04-programs/daemons.md#kdos-boxsock) and [The
security model](../03-architecture/security-model.md#sandboxed-clients).

### self-hosting

Able to build itself. From `20_selfhost` onwards the KDOS build is self-hosting: `20_selfhost`
rebuilds the C library, the compiler and their companions with the compiler `10_bootstrap` made,
inside the *chroot*, and every later phase is built by those tools. `30_foundation` and
`31_compilers` install the build systems, interpreters and large compilers, and `40_lang` the
remaining languages and build tools, such as `scons`, `gn` and `ocaml`, so that from the end of
`40_lang` KDOS can rebuild KDOS. See [How KDOS is
built](../05-developer/how-kdos-is-built.md).

### session

A running Plasma desktop for one person: KWin, `plasmashell`, the session manager `ksmserver`,
`kded6` and what they start, on the session bus at `$XDG_RUNTIME_DIR/bus`. SDDM starts one through
`kdos-session-wrapper` and `startplasma-wayland`. The person's *user services* (the bus, PipeWire,
the timers) are not part of it: they start at the first login of any kind and outlive a session. See
[The session](../03-architecture/session.md).

### shelf

The subject directory an upstream *port* is filed under: `ports/core/<shelf>/<name>/`, such as
`ports/core/kde/kwin/`. There are 102, a closed list kept in `ports/shelves` with one line each
giving the shelf's source-archive volume and saying what belongs on it. The shelf is only where the recipe is filed: a port is named by its bare
name everywhere, and no recipe key records the shelf, so moving a port between shelves changes
no hash and no package: only its path, and the shelf file of its phase's package list that names
it. The *package lists* and [the ports catalogue](ports-catalogue.md) are grouped
by shelf. A shelf is neither a *group* nor a *ring*, and KDOS's own ports are not shelved; they sit
in *areas* under `src/`. See [Packaging](../03-architecture/packaging.md#repositories-and-shelves)
and
[Decisions](../01-philosophy/decisions.md#ports-are-shelved-by-subject-identity-is-the-bare-name).

### shim

A symbolic link named after a boxed application and pointing at `/usr/local/bin/kdos-appbox`, which
looks up the name it was started by in the **`alien-apps`** table, the map from an application name
to the command that runs it inside its box. It is what makes a boxed application an ordinary
command. Shims for applications baked into the image live in `/usr/local/bin`, with their table at
`/usr/share/kdos/alien-apps`; those for applications you install live in `~/.local/bin`, with
`~/.local/share/kdos/alien-apps`. See [kdos-appbox](../04-programs/kdos-appbox.md#two-trees).

### slot

A named colour role in `libkcolor` that resolves against the current *accent*. There are eight:
`KT_BG`, `KT_ERR`, `KT_ACCENT`, `KT_WARN`, `KT_DIM`, `KT_MID`, `KT_SURFACE` and `KT_TEXT`. KDOS's
terminal programs and the boot splash take every colour from a slot rather than from a literal, and
the generators map the same slots onto the roles of a KDE colour scheme, so the desktop and the
console agree. (For the A/B root filesystems, see *root slot*.) See [The design
language](../03-architecture/design-language.md).

### slot image

A whole *root slot* as one signed release: a manifest listing every file with its hash, and the
files' contents in bundles, with a *delta* from each earlier version. `kdos update apply` writes one
into the inactive slot, copying every file the running root already holds and downloading the rest.
`kdos image` makes, verifies and unpacks them. See [Discover](../04-programs/discover.md#system-updates).

### snapshot

An archive of a completed build *phase*'s result under `build/snapshots/<phase>/`: one compressed
`tar` per declared path and a `manifest.json`. `00_cross` and `10_bootstrap` archive `cross`, `fs`
and `mark`; the package phases up to `50_desktop` archive `fs`; `60_kernel` and `70_image` take
none. Each path is archived whole or as a *snapshot layer* on an earlier snapshot. A later build
restores a snapshot and continues from the phase after it instead of starting again. See
[The build system](../05-developer/build-system.md#snapshots).

Two unrelated uses share the word. The catalogue's `snapshot` key pins the date of the Debian
archive that boxes are built from (see [Packs and
boxes](../03-architecture/packs-and-boxes.md#the-catalogue)), and `kdos-box snapshot <box> [tag]`
saves a tagged copy of one box's writable layer (see
[kdos-appbox](../04-programs/kdos-appbox.md#snapshots-and-rollback)).

### snapshot layer

A *snapshot*'s archive of one path that holds only what changed since another snapshot, its base:
every entry that is new, changed type, or has a new inode or ctime, the directories those sit in,
and a `.gone` list of what was removed. The changes are found by comparing a walk of the tree with
the index in `build/.snap-lineage/`. Restoring a layer extracts the full archive at the bottom of
its chain and every layer above it in turn. `--full-snapshots` writes none. See
[The build system](../05-developer/build-system.md#layers-and-full-snapshots).

### source archive

The store of every upstream source file the recipes name: release assets of the GitHub repository
`kunaldawn/kdos`, in numbered pre-releases, the volumes `sources-1`, `sources-2` and on, each holding
the shelves `ports/shelves` maps to it. An asset is named `<shelf>--<file>` (longer when that name
is already taken in the volume), or `attic--<file>` for a file only old history names, and each
file is checked against its SHA-256 before use. A file larger than 1,900 MiB is stored in parts. Nothing in it is replaced, and nothing is removed except by
`ports/publish --rehome` and `--prune=yes-delete`. `make fetch` reads it through
*sources.idx*; `ports/publish` adds to it. See [Packaging](../03-architecture/packaging.md#where-sources-come-from) and [Writing
ports](../05-developer/writing-ports.md#publishing-sources).

### source cache

`ports/.srccache`, or wherever `KDOS_SRCCACHE` points: every source file `make fetch` has verified,
stored as `sha256-XX/<hash>` (`XX` being the hash's first two hex digits) and hard-linked into the
port directories. A branch switch downloads nothing twice; a second checkout does the same only when
`KDOS_SRCCACHE` points both at one cache. The book also calls it the **srccache**. It is plain data
that any C library can read, so it survives a switch between building in a container and building on
the host. See [Packaging](../03-architecture/packaging.md#where-sources-come-from).

### source hash

The `sha256 =` line in a recipe, naming one source file by the SHA-256 of its contents. It is the
file's identity: a copy that matches is the right file wherever it came from. See [Writing
ports](../05-developer/writing-ports.md#checksums).

### sources.idx

`ports/sources.idx`, the committed index of the *source archive*, in format 2: the line
`# kdos-sources-index 2` first, then one line per file, `<sha256> <tag> <asset> <port>/<file>`, with
`parts=<N>:<h1>,…` for a file stored in parts, giving the release and asset each hash lives in.
`make fetch` and the pre-push hook read it from the tree, so a file uploaded to the archive is found
only once its line is committed, and an index of any other format is not read at all. See [Writing
ports](../05-developer/writing-ports.md#publishing-sources).

### splash

`kdos-splash`, the boot animation. It draws a fade from black, or with *Phosphor mode* a CRT
power-on, and a list of boot stages straight to the framebuffer, `/dev/fb0`. The *initramfs* starts
it, and at the end of *rcS* it plays its power-off and hands the screen, black, to SDDM's greeter. Every failure path exits without
drawing, so the splash cannot stop a boot. See [Boot and init](../03-architecture/boot-and-init.md#the-splash).

### store

The store *lane*: building an application from the *catalogue* on your machine, which `kdos-appbox
install` does and Discover's KDOS backend asks for. Not to be confused with the *pack store*,
`/var/lib/kdos/packs`, where installed packs are kept, or with KDOS's *source archive*. See
[Discover](../04-programs/discover.md#the-kdos-backend).

### surface

One window or popup of the desktop rather than of an application: a panel, the Start menu, a tray
applet's popup, a notification, the lock screen. On KDOS they are drawn by the Plasma shell and by
KWin. See [The Plasma shell](../04-programs/plasma-shell.md).

### sysupdate1

`org.freedesktop.sysupdate1`, the D-Bus interface Discover drives an operating-system update
through. On KDOS `kdos-sysupdated` serves it over the *root slots*: checking for a newer version,
acquiring it into the inactive slot and arming that slot for the next boot. See
[Discover](../04-programs/discover.md#kdos-sysupdated).

### sysvinit

The System V init package that provides PID 1: `init`, `telinit`, `shutdown`, `halt`, `reboot`,
`poweroff`, `runlevel` and `killall5`. It reads `/etc/inittab`, runs *rcS* at boot, keeps the
consoles running in *runlevel* 3 and runs *rcK* and the rest of the shutdown entries in runlevels
0 and 6. It supervises no service; *ksvc* does. See [Boot and
init](../03-architecture/boot-and-init.md#rcs-and-the-service-scripts).

### target tree

`build/fs`, the root filesystem the build assembles and every phase from `20_selfhost` onwards runs
in as its *chroot*. `70_image` turns it into the ISO. See [The build
system](../05-developer/build-system.md#the-chroot).

### trial boot

A boot of the candidate *root slot* after an update, before it is confirmed. On UEFI, `kdos-bootctl
try` arms one boot through the firmware's one-shot `BootNext` variable, with a copy of the loader
under `EFI/kdos/trial/` on the *ESP*; without UEFI variables the boot menu leads with the candidate
for a number of boots (default 3). A candidate whose trial is spent without its *bless* is rolled
back. See [Boot and init](../03-architecture/boot-and-init.md#one-boot-through-bootnext).

### turnstile

`turnstiled`, the daemon that starts a user's *user services* at that user's first login and stops
them after the last, and makes the runtime directory `/run/user/<uid>`. Its backend on KDOS is
*ksvc*. See [The session](../03-architecture/session.md).

### user service

A program run as one user for as long as that user is logged in anywhere, from
`/etc/kdos/user-services/`: the session bus, PipeWire, WirePlumber, `pipewire-pulse`, mpd, the user
timers and the boxes' stop at logout. *turnstile* starts them through *ksvc*; `service --user`
lists, starts and stops them. See [The session](../03-architecture/session.md).

### vendor bundle

A tarball of a port's language dependencies (Rust crates, Go modules, Python source distributions,
Hackage packages or npm packages), named `<name>-vendor-<version>.tar.xz`, so that the build needs no network. `make
fetch` takes it from the port directory, the *source cache* or the *source archive* like any other
source, and generates it, by default in the `kdos-fetch` container, only when none of them has it.
It is generated reproducibly, so it has a stable *source hash*. A recipe asks for one with the
`vendoring =` key; 168 ports in `ports/core` set it (80 Rust, 39 Go, 43 Python, 4 Haskell, 2 Node). See
[Writing ports](../05-developer/writing-ports.md#vendoring).

### warmup

Starting the boxes named in `~/.config/kdos/favorites` in the background at login (`kdos-appbox
warmup`, run by `kdos-session-wrapper` before Plasma starts), so the first launch of a boxed
application does not wait for its container to start. See
[kdos-appbox](../04-programs/kdos-appbox.md#warmup-and-collection).

### whiteout

How a filesystem layer records a deletion. Overlay whiteouts are kept in `trusted.overlay.*`
attributes that only root can write, so `kdos-pack build` must run as root for `mkfs.erofs` to
preserve them, and `kdos-appbox export` flattens a box with `podman export` so that no whiteout is involved.
Because `kdos-packd` mounts as root, an application pack can delete a file its base provides. See
[kdos-appbox](../04-programs/kdos-appbox.md#export-and-import) and [Packs and
boxes](../03-architecture/packs-and-boxes.md#composition).

### worker

A KIO worker: the separate process KIO runs to reach one kind of location, `kio_sftp` for `sftp:/`,
`kio_library` for `library:/`. See *KIO*.

### xdg-foreign

The Wayland protocol that lets one client name another client's window by a handle. The portal
backend imports a dialog's parent window through it, so a boxed application's file dialog opens over
the window that asked. See [The session](../03-architecture/session.md#portals).

### Xwayland

An X server that runs as a Wayland client. KWin runs it rootless, starting it when the first X11
client connects, so X11-only applications, native or in boxes, work. It serves GLX through Mesa's
`libGLX_mesa` behind `libglvnd`, so an X11 client that draws through GLX gets OpenGL; Wayland
clients draw through EGL. It is the one X server permitted on the host. See
[KWin](../04-programs/kwin.md#xwayland) and
[Principles](../01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## Words this book avoids

The book keeps to one vocabulary. These words either blur a distinction the entries above draw or
promise something the text cannot support.

| Not used | Instead |
|---|---|
| "app store" | *Discover*, for the window; the store *lane*, for building an application from the catalogue on your machine. There is no account and no purchase |
| "distro" in prose | Distribution |
| "just" | Say the thing without it. "Just run X" hides how much X is |
| "simply", "obviously", "of course" | Omit them; state the fact without judging its difficulty |
| "should work" | Say what was measured, or say it is untested |
| First-person decision narration | State the decision and its reason in the present |
| Past-tense narration of any kind | These pages describe the present. See [Principles](../01-philosophy/principles.md#documentation-describes-the-present) |
| "sandbox" for a box, unqualified | A box limits what an application can do to the desktop, not to your data |
| "group", unqualified, in build prose | *The `group =` key*, or *shelf* where the grouping of the ports tree or a package list is meant |
| "category" for where a port is filed | *Shelf*. "Category" belongs to the catalogue, the pack index and desktop entries |
| "widget" for a KDOS settings page | *KCM*, or the page's name in System Settings; a widget is a *plasmoid* |

## See also

- [Architecture overview](../03-architecture/overview.md) — where most of the running-system terms
  live
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the build terms in the order a build
  meets them
- [How KDOS differs](../01-philosophy/how-kdos-differs.md) — the host's components beside their
  counterparts in other distributions
- [The ports catalogue](ports-catalogue.md) — every port the *port* and *shelf* entries count
- [Principles](../01-philosophy/principles.md) — the rules behind the vocabulary
- [Command index](command-index.md) — every command by name
- [Configuration](configuration.md) — every setting by name

<!-- book-nav -->
---

*Part VI — Reference, chapter 45.* Previous: [44. Status](status.md) · [Contents](../README.md)
