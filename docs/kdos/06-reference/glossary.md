# Glossary

This chapter defines the vocabulary the rest of the book uses, one entry per term, in alphabetical
order. Each entry is a heading, so another chapter can link one term directly
(`glossary.md#layer-surface`). It is for any reader who meets a word in another chapter and wants
its exact meaning; it assumes nothing beyond general familiarity with Linux. Where a term has a
general meaning elsewhere and a narrower one in KDOS, the entry gives the KDOS meaning, which is the
one these pages use. Each entry links the chapter that treats the term in depth. A word printed in
*italics* inside an entry has an entry of its own. No other chapter is needed first, though
[Architecture overview](../03-architecture/overview.md) and [How KDOS is
built](../05-developer/how-kdos-is-built.md) give most of these terms their context, and [How KDOS
differs](../01-philosophy/how-kdos-differs.md) sets the host's components (*musl*, *toybox*,
*seatd*, the *labwc* fork) beside the ones other distributions use.

The section after the terms lists words the book avoids, and what it says instead.

## Terms

### accent

One of the eight palettes: phosphor, amber, ice, bone, norton, borland, perfect and paper, defined
in `src/libs/libkcolor/kcolor.h`. An accent is a small set of related values, not a single colour,
and every drawn surface resolves its *slots* against it. The current accent is stored as one word in
`$XDG_CACHE_HOME/kdos/theme` (normally `~/.cache/kdos/theme`), which every KDOS surface reads to
choose its palette; the colours themselves are compiled in. See
[Theming](../02-user-guide/theming.md).

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
`desktop` (programs that draw the session or serve it), `daemons` (root daemons the desktop account
talks to), `system` (the package manager, the `kdos` command, packs, boxes and the installer),
`art` (themes, pictures and their generators), `libs` (the `libk*` libraries) and `devtools` (build
tools that are never installed). Every port of KDOS's own sits exactly at `src/<area>/<name>/`, so
that `../../libs` from it is `src/libs`. The four areas that hold recipes are *port* repositories;
`libs` and `devtools` hold none. An area is not a *shelf*. See
[Packaging](../03-architecture/packaging.md#the-recipes-under-src) and
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

The **binary host**: a directory of prebuilt host packages with a signed index, read by `kpkg
binhost`. It is a local path, such as a USB stick or an NFS mount, never a URL. It is optional and
one you make yourself; there is no public one. A package from it is used only when its architecture,
its *build-config hash* and its *recipe hash* all equal this machine's. See
[Packaging](../03-architecture/packaging.md#the-binary-host).

### box

A rootless podman container that one application, or one working environment, runs in. A **store
box** is a distrobox container created from the image the store *lane* built; a **pack box** is
created over an overlay of mounted *packs* plus a writable layer. A box is a packaging and desktop
boundary, not a security boundary against you: it shares your home directory. See [Packs and
boxes](../03-architecture/packs-and-boxes.md#the-box) and [The security
model](../03-architecture/security-model.md).

### box chip

A small square in a box's colour at the left of a window's title, drawn by the compositor for a
window whose *security-context-v1* tag names a box. A box gets a colour from `accent =` in its *box
profile*; a box in the session's own accent draws no chip, so a default install shows none. The chip
identifies the box; it does not show focus. See
[kdos-comp](../04-programs/kdos-comp.md#the-box-chip).

### box profile

`~/.config/kdos/boxes/<name>.conf`, a handful of settings for one box. Every key maps onto a
container-engine flag or onto something KDOS enforces itself, and the profile printer says which.
See [kdos-appbox](../04-programs/kdos-appbox.md).

### build-config hash

One of the two hashes that decide whether a prebuilt package matches this machine: a SHA-256 over
the architecture, the target, the C library, the compiler and its version, and the compiler flags.
Written `B:` in a package index. See also *recipe hash*, and
[Packaging](../03-architecture/packaging.md#b--the-build-config-hash).

### catalogue

The shipped list of every application KDOS knows how to build, as Debian packages:
`src/system/kdos-appbox/catalogue`, installed as `/usr/share/kdos/appstore/catalogue`. Its rows
are *bases*, *runtimes*, applications, data sets and groups of applications. Not to be confused with
[the ports catalogue](ports-catalogue.md), the chapter that lists every host *port*. See [Packs and
boxes](../03-architecture/packs-and-boxes.md#the-catalogue).

### cell grid

The model every KDOS surface is drawn in: a two-dimensional buffer of character cells, each with a
character, a foreground slot, a background slot and attributes. Every surface this project paints is
one: the panel and all its surfaces, the resource monitor, the terminal, the lock screen and the
installer. `libktui` draws the grid and presents it through a backend: a terminal (including the
Linux console on `tty1`), a Wayland window under the compositor, or an offscreen *dump*. Three
things on screen are not cells: the compositor's own window chrome (see *chrome*), drawn with pango
at a size matched to the cell; the pixel layer (plates, rules, rounded ends and display text) a
Wayland-backed surface may paint beneath its cells; and whatever an application in a box draws for itself. The boot
*splash* is not a cell grid either: it draws pixels straight to `/dev/fb0`. See [The design
language](../03-architecture/design-language.md).

### chrome

The furniture around content inside a KDOS surface: the frame, the header band, group headings, the
button bar and the scrollbars. It is drawn in cells by `libkchrome`, so there is one implementation
of each. Distinct from **window chrome**, the titlebar, the root menu and the window-switcher
display, which the compositor draws itself with pango. **Supervised chrome** is a third sense: the
desktop programs the compositor starts and restarts. See [The
session](../03-architecture/session.md).

### chroot

A process whose root directory has been changed, so that it sees one directory tree as `/`. Every
build phase from `20_selfhost` onwards runs each of its commands inside the *target tree* `build/fs`
through `script/chroot/exec.sh`, with a cleared environment to which it adds only the variables it
names, so a build uses only the compilers, libraries and tools that earlier phases put there and
nothing from the build container. `script/chroot/enter.sh` is the interactive counterpart, for
inspecting the tree by hand; the build does not use it. See [The build
system](../05-developer/build-system.md#the-chroot).

### coast

The scroll that continues after a finger leaves a touchpad while still moving: `libkwl` keeps
producing the wheel ticks the finger would have made, slowing to a stop, unless a scroll, a click, a
key or the pointer leaving ends it first. Off with `motion = no`. See [Writing desktop
software](../05-developer/writing-desktop-software.md#input-the-backend-cleans).

### commit

In Wayland, the request by which a client makes the state it has attached to a surface, its buffer
among it, take effect; nothing a client changes on a surface is shown until it commits. See
*configure*, and [Writing desktop
software](../05-developer/writing-desktop-software.md#choosing-a-role).

### compose

To build a pack box's root filesystem as an overlay of its pack stack, which `kdos-packd` does on
request. Composing is safe to repeat, because mounts are reference counted, and it is redone before
every start, because the overlay lives on a temporary filesystem that a reboot empties. See [Packs
and boxes](../03-architecture/packs-and-boxes.md#composition).

### configure

In Wayland, the compositor's message that gives a surface its size and state. The client
acknowledges it, then draws and *commits*. The acknowledgement quotes the configure's **serial**,
the number the compositor attaches to a configure or an input event so that a later request can show
which event it answers. See [Writing desktop
software](../05-developer/writing-desktop-software.md#choosing-a-role).

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

A binary difference between two packages or two packs, made with `zstd --patch-from`. A package
delta is taken over the uncompressed tars; a pack delta is taken over the two packs as they are,
which works because *EROFS* compresses each cluster separately. A delta carries no signature of its
own. `kpkg binhost` checks a rebuilt package against the hash the signed index already carries and
falls back to the full package on a mismatch; `kdos-pack apply` rebuilds a pack without checking it
and leaves the comparison with the index's `C:` hash to you. See
[Packaging](../03-architecture/packaging.md#deltas) and [Packs and
boxes](../03-architecture/packs-and-boxes.md#deltas).

### display text

A heading or a figure drawn on the pixel layer in whole rows of the cell height, in the cell font's
face, such as the number at the right of `kdos-res`'s header band. Where there is no pixel layer it
is drawn as cells on the first row of its rectangle, or not at all when the cells already say it.
See [Writing desktop software](../05-developer/writing-desktop-software.md#display-text).

### dockapp

One gadget in the *slit*: a command re-run on an interval whose output is one line of text. See
[kdos-comp](../04-programs/kdos-comp.md#keys-applied-at-the-next-login).

### dump

One frame of a surface printed as text instead of drawn on a display, with `--dump` (the characters)
or `--dump-cells` (every cell's character, colours and attributes). *Goldens* are made of dumps. See
[Testing](../05-developer/testing.md#goldens).

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
The panel has one; the background does not. See [Writing desktop
software](../05-developer/writing-desktop-software.md#choosing-a-role).

### fixture

Recorded system state that a program can be pointed at instead of the live machine, which is what
makes its readings and decisions testable. There are 49 fixture directories under
`testing/fixtures/`. See [Testing](../05-developer/testing.md#fixtures).

### front end

One of the programs inside the single `kdos-shell` binary: a name with its own entry point, chosen
by the name the binary is started under. Most new *surfaces* are a new front end. See [Writing
desktop software](../05-developer/writing-desktop-software.md#adding-a-name-to-kdos-shell).

### glide

A scrolled list presented sliding to its new rows over 100 ms rather than jumping. The list's
cells have already moved by whole rows; only the pixels of the frames in between lag behind them,
and `libkwl` commits those frames itself. See [The design
language](../03-architecture/design-language.md#motion).

### glyph tier

Which set of drawing characters a surface may use, chosen by `libktui` from the terminal's
capabilities. There are three: **rich** (UTF-8 outside the Linux console, with eighth-block bars),
**vt** (UTF-8 on the Linux console, with three-level bars) and **ascii** (no UTF-8). The vt tier
exists because the console font has 512 glyphs, and a character it lacks is drawn as a blank. See
[The design language](../03-architecture/design-language.md#the-glyph-tiers).

### golden

A committed reference frame: a surface's *dump*, compared byte for byte by the self-test. Text
frames catch geometry; cell frames catch colour as well. There are 217 under `testing/goldens/`,
counting every file there except its `README`. See [Testing](../05-developer/testing.md#goldens).

### graft

A declared placement of a *data pack*'s contents where a consumer will look for them. There are two
namespaces, because the host's data directories are invisible inside a box. Each graft is recorded
in a manifest, so that removal is exact. `kdos-packd` places a graft when asked `graft <id>`; no
program in the tree asks, so a data pack is grafted by hand. See [Packs and
boxes](../03-architecture/packs-and-boxes.md#grafts-and-data-packs).

`testing/selftest.sh` and the compositor's source comments also call `kdos-comp`'s own files,
`src/desktop/kdos-comp/src/kdos-*.c`, "graft files". The book calls those the compositor's
**additions** (see *labwc*) and keeps *graft* for data packs.

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
toybox, bash, a module set and the programs early boot needs. See [Boot and
init](../03-architecture/boot-and-init.md#the-initramfs).

### kdos-packd

The root daemon that is the only program on the system to mount a *pack*. It verifies and installs
packs, mounts them, *composes* a pack box's root from them and places *grafts*, and answers root and
members of `wheel` only. `kdos-appbox` is its client. See [The
daemons](../04-programs/daemons.md#kdos-packd).

### key chord

A key combination pressed together, such as `Super+Escape`. The compositor binds its chords in
`~/.config/kdos-comp/rc.xml`, copied from `/etc/skel` for each new account, and `kdos-palette` finds
them by name. A chord that only runs a command reaches a place through a *route*. See
[Configuration](configuration.md#configkdos-comprcxml).

### kimpanel

The D-Bus panel protocol through which an input-method engine such as fcitx5 hands its candidate
list and *preedit* to a separate window. `kdos-ime` speaks it, so the candidate window is drawn in
cells like the rest of the desktop. See [kdos-shell](../04-programs/kdos-shell.md#kdos-ime) and [The
session](../03-architecture/session.md#input-methods).

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

### ksvc

The program from `kdos-tools` that supervises the root daemons, restarting one five seconds after it
exits and sending its output to syslog. Under the name `service` it lists, starts, stops, enables
and disables the service scripts `/etc/init.d/NN_<name>.sh`. The same binary, installed as
`/usr/sbin/ksvc`, is also `kdos`, `kdos-getty`, `kdos-bootctl` and other commands, each a separate
program chosen by the name it is started under. See [The
daemons](../04-programs/daemons.md#supervision-and-logging) and [The kdos
command](../04-programs/kdos-command.md#ksvc-and-service).

### labwc

The Wayland compositor that `kdos-comp` is a frozen hard fork of, taken at labwc 0.20.0: its source
tree renamed and extended in place, with twenty-three KDOS additions each in its own `src/kdos-*.c`
file, and no upstream changes merged. See [kdos-comp](../04-programs/kdos-comp.md) and
[Decisions](../01-philosophy/decisions.md#the-compositor-is-a-frozen-fork-of-labwc).

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
of an output, on a layer above or below the windows, rather than as a window. The panel and the
compositor's other per-output children are layer surfaces; one that names no output is placed on one
screen only, which is why the compositor starts one of each per output. The strip it reserves is its
*exclusive zone*. See [Writing desktop
software](../05-developer/writing-desktop-software.md#choosing-a-role) and
[kdos-comp](../04-programs/kdos-comp.md#supervised-children).

### libkdisp

The library that decides which display server a surface reaches. It defines a surface's lifecycle as
an interface and names no implementation; the consumer passes in the ones it links. One ships,
`kwl_impl` in *libkwl*. The interface is also the seam that lets *goldens* be drawn with no
compositor. See [The C libraries](../05-developer/c-libraries.md#libkdisp).

### libkwl

`libktui`'s Wayland backend: it paints the same *cell grid* into a shared-memory buffer with fcft
and gives a surface its role (a layer surface, an xdg-shell window or a session lock). It is a
separate source directory, so a program with no Wayland window never compiles it in. That keeps
`libktui` free of any link beyond the C library, which lets the installer compile `libktui` in the
`10_bootstrap` phase. See [The C libraries](../05-developer/c-libraries.md#libkwl).

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
Firefox ESR, LibreOffice, GIMP or Kate. It links the host's toolkits (GTK, Qt, KDE Frameworks,
wxWidgets, FLTK, Tk or SDL), which are ports too. Each toolkit that has a Wayland backend is built
with it as the run-time default and its X11 backend compiled in beside it; Tk has only X11, so a Tk
window is an *Xwayland* client. It runs with no *box*, next to KDOS's own *surfaces*, which link
none of those toolkits. The opposite is an *alien app*. Most are built in the `43_toolkits` and
`44_apps` phases; see [The ports catalogue](ports-catalogue.md) and
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
import passes through and the `mnt` directory packs are mounted under. Not to be confused with the
*store* (`kdos-store`) or the store *lane*. See [Packs and
boxes](../03-architecture/packs-and-boxes.md#installing-a-pack).

### package list

The list of ports a *phase* installs: one port name per line, with `#` comments. A phase keeps it
either in one file, `packages.txt`, or in a directory, `packages.d/`, whose `*.txt` files are read
in byte order as one list; never both. Ten phases have one, from `20_selfhost` to `60_kernel`. The
five userland phases, `40_lang` to `44_apps`, use `packages.d/`: one `<shelf>.txt` per *shelf* the
phase draws on, one `src-<area>.txt` for the ports it builds from an *area*, and, in `40_lang` and
`41_system`, an `00-order.txt` that sorts first and holds the runs whose order a comment pins. A
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
has its environment in its own directory, `phase.env`, which says whether it runs in the *chroot*,
which port repositories it searches beyond the default `/ports/core` and what its *snapshot* holds, and which sources the settings
every phase shares from `script/env/`. See [The build
system](../05-developer/build-system.md#phases).

### phosphor pass

The compositor's CRT-imitating shader over the whole desktop: a horizontal bleed, a vignette, a
faint floor so that black is never quite black, and optional scanlines and curvature. It runs only
under the GLES2 renderer, so a virtual machine with plain graphics never shows it. Not to be
confused with `phosphor`, one of the eight *accents*. See
[kdos-comp](../04-programs/kdos-comp.md#the-phosphor-pass).

### port

One piece of host software as this repository describes it: a directory holding a *kpkgbuild* and a
`build.sh` (the build, run by bash with the unpacked source as its working directory). There are
five port repositories in one format, holding 2,024 recipes: 2,000 upstream ports in `ports/core/`,
each on a *shelf*, and 24 of KDOS's own in four *areas*: 5 in `src/system/`, 6 in `src/art/`, 8 in
`src/desktop/` and 5 in `src/daemons/`. (The sixth directory in `src/system/`, `kdos-kpkg`, has no
recipe; `10_bootstrap` compiles it by script.) A port is known by its bare name, which is unique
across all five. See [Writing ports](../05-developer/writing-ports.md) and [The ports
catalogue](ports-catalogue.md).

### portal

How a sandboxed application asks the host to do something it cannot do itself, such as open a file
chooser, share the screen or open a link. The front-end daemon `xdg-desktop-portal` owns the public
D-Bus interfaces, and backends, among them KDOS's own `xdg-desktop-portal-kdos`, implement them. See
[The session](../03-architecture/session.md#portals).

### preedit

The text an input method is still composing, shown before it is committed to the application. See
*kimpanel*.

### preflight

`testing/preflight.sh`, the check that the tree's wiring is consistent (every package resolves,
every recipe parses, every script is valid) without building anything. See
[Testing](../05-developer/testing.md#preflightsh).

### PSI

Pressure stall information: the kernel's measure, under `/proc/pressure/`, of the time tasks spend
stalled waiting for CPU, memory or I/O. `kdos-oomd` acts on a memory-pressure trigger, and the
resource monitor shows the readings. See [The daemons](../04-programs/daemons.md#kdos-oomd).

### quantum

The number of audio frames PipeWire processes in one cycle, which sets the audio latency. In a
virtual machine only, `/etc/pipewire/pipewire.conf.d/99-kdos-vm.conf` raises it to 4096 with a
ceiling of 8192. [kdos-bb](../04-programs/kdos-bb.md) uses the word for one mixer buffer. See [The
session](../03-architecture/session.md#the-quantum-and-why-a-virtual-machine-gets-a-bigger-one).

### RAPL

Running Average Power Limit, the processor's energy counters. Only root can read them, so
`kdos-energyd` reads them and the resource monitor's Energy page asks it. The figures are shares,
never watt-hours, because RAPL cannot see the panel, the radio or the disk. See [The
daemons](../04-programs/daemons.md#kdos-energyd).

### rcK

`/etc/init.d/rcK`, the shutdown counterpart of *rcS*: the first `::shutdown` entry in
`/etc/inittab`, which stops the service scripts in reverse numeric order. See [Boot and
init](../03-architecture/boot-and-init.md#shutdown).

### rcS

`/etc/init.d/rcS`, the script toybox's `init` runs once at boot as its `sysinit` entry. It mounts
the filesystems, runs each enabled service script `/etc/init.d/NN_<name>.sh` in numeric order, runs
`kdos-bootctl mark-good` and quits the *splash* before the login prompt. See [Boot and
init](../03-architecture/boot-and-init.md#rcs-and-the-service-scripts).

### recipe

The files that describe how to build one *port*: its *kpkgbuild*, its `build.sh`, and any
`postinstall.sh` and patches beside them. The *recipe hash* is taken over them and every other
file beside them that no `sha256 =` line names. See [Writing
ports](../05-developer/writing-ports.md).

### recipe hash

The other of the two package hashes: a SHA-256 over a port's recipe files (`kpkgbuild`, `build.sh`,
`postinstall.sh` and every patch) and every other file in its directory that no `sha256 =` line
names, so editing a kernel config beside a recipe changes it. For a port with no `source =` that is
the port's whole directory, and for one of KDOS's own ports under `src/` all of `src/libs` as well, so editing a
program's source or a shared library changes it. Written `E:` in a package index. It decides what
the build rebuilds. Not to be confused with a *source hash*. See
[Packaging](../03-architecture/packaging.md#e--the-recipe-hash).

### restamp

Two operations share the word. `kdos-pack restamp` raises a *pack*'s footer to the current format
and takes its digest again over that format's span, dropping the signature block, so the pack must
then be signed and indexed again; it is the repair for a pack that fails its digest check.
`kdos-bootctl theme <accent>` restamps an installed machine's boot menu and console palette in
place. See [Packs and boxes](../03-architecture/packs-and-boxes.md#rules-the-format-keeps) and [Boot
and init](../03-architecture/boot-and-init.md#restamping-an-installed-machine).

### retint

To re-read the theme and repaint in its colours. A running surface retints on `SIGHUP`, which `kdos
theme` and `kdos toggle night-light` send; a program with no `SIGHUP` handler is not signalled.
Most `kdos-shell` front ends are not sent it and poll the theme file's time instead, once per pass
of their loop; every front end on the surface runner does. A front end that does neither keeps the
colours it opened with until it is closed. See
[Theming](../02-user-guide/theming.md) and [Filesystem and IPC](filesystem-and-ipc.md).

### rig

The virtual-machine harness that boots a real KDOS image in QEMU, drives it with keys, pointer and
commands, and photographs the screen: `testing/vnc-shot.py` and the scripts around it. See
[Testing](../05-developer/testing.md#the-qemu-rig).

### ring

Which of three divisions of the software a program belongs to: core (`ports/core/`), desktop (the
*areas* under `src/` other than `devtools`, whose build-machine tools belong to no ring) or outer (the *catalogue*). A *shelf* subdivides the core ring; it is not a
ring. Build cost and ownership decide the ring. See [Architecture
overview](../03-architecture/overview.md#the-three-rings).

### root slot

One of the two root filesystems, A and B, on an installed disk. One is live; an update is written
into the other and tried on the next boot, and `kdos-bootctl` selects and confirms slots. See [Boot
and init](../03-architecture/boot-and-init.md#ab-slot-selection).

### route

A stable name for a place in the system, written `verb.noun` (`setup.network`), defined in
`/etc/kdos/menu.conf` or `~/.config/kdos/menu.conf` and opened with `kdos menu summon <route>`. It
keeps working when the chord is rebound or the menu row moves. See [The kdos
command](../04-programs/kdos-command.md#kdos-menu).

### runtime

A layer shared by many applications, holding a toolkit or a family of libraries, such as `rt-gtk` or
`rt-qt`. The catalogue names seven. An application row names a runtime or a base as its parent. See
[Packs and boxes](../03-architecture/packs-and-boxes.md#the-catalogue).

### scene

In the compositor, the wlroots scene graph: the tree of buffers composited into each frame. In
[kdos-bb](../04-programs/kdos-bb.md), one timed part of the demo. See
[kdos-comp](../04-programs/kdos-comp.md#how-it-is-implemented).

### seatd

The seat daemon that grants the session the display and input devices without systemd-logind. It
runs as the supervised service `/etc/init.d/45_seatd.sh` with `-g seat`, so members of the `seat`
group take a seat without root. See [The daemons](../04-programs/daemons.md#who-may-ask-for-what)
and [How KDOS differs](../01-philosophy/how-kdos-differs.md#init-and-service-supervision).

### security-context-v1

The Wayland protocol through which a process hands the compositor a listening socket to tag. Every
client that connects on it carries the tag (an engine name, an application id and an instance id),
which the client can neither see nor change. `kdos-boxsock` creates one socket per box, tagged
`io.kdos.appbox` with the box name; the compositor's filter for sandboxed clients, the *box chip*
and the panel read that tag. See [The daemons](../04-programs/daemons.md#kdos-boxsock) and [The
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

A running desktop: the compositor, its supervised chrome, and the per-user services under it. The
login shell on `tty1` starts one through `kdos-desktop`, and no other tty does; a session that fails
to come up falls back to that shell's prompt rather than taking the terminal with it. See [The
session](../03-architecture/session.md).

### shelf

The subject directory an upstream *port* is filed under: `ports/core/<shelf>/<name>/`, such as
`ports/core/wl/wlroots/`. There are 102, a closed list kept in `ports/shelves` with one line each
saying what belongs on it. The shelf is only where the recipe is filed: a port is named by its bare
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

### slit

`kdos-slit`, the dockapp column: one line of text per *dockapp*, configured in
`~/.config/kdos/slit.conf` as `<interval_s> <width_cells> <command> [args…]` per line. It is off by
default; `slit = yes` in `~/.config/kdos/comp.conf` makes the compositor start one per output at the
next login. See [kdos-comp](../04-programs/kdos-comp.md#keys-applied-at-the-next-login) and
[Configuration](configuration.md#configkdosslitconf).

### slot

A named colour role that resolves against the current accent. There are eight: `KT_BG`, `KT_ERR`,
`KT_ACCENT`, `KT_WARN`, `KT_DIM`, `KT_MID`, `KT_SURFACE` and `KT_TEXT`. Everything drawn takes its
colour from a slot rather than from a literal value. (For the A/B root filesystems, see *root slot*.
A *sprite* occupies a numbered slot of `libktui`'s sprite table, an unrelated use.) See [The design
language](../03-architecture/design-language.md).

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
`kunaldawn/kdos`, one pre-release per shelf tagged `src-<shelf>` plus `src-attic` for files only
old history names, each asset under the file's own name and each file checked against its SHA-256
before use. A file larger than 1,900 MiB is stored in parts. Nothing in it is replaced, and nothing is
removed except by `ports/publish --rehome` and `--prune=yes-delete`. `make fetch` reads it through
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
only once its line is committed, and an index of any other format is not read at all. It names no
file until `ports/publish` fills it. See [Writing
ports](../05-developer/writing-ports.md#publishing-sources).

### splash

`kdos-splash`, the boot animation. It draws a CRT power-on animation and a list of boot stages
straight to the framebuffer, `/dev/fb0`, rather than in cells. The *initramfs* starts it, and *rcS*
quits it just before the login prompt. Every failure path exits without drawing, so the splash
cannot stop a boot. See [Boot and init](../03-architecture/boot-and-init.md#the-splash).

### sprite

A picture occupying whole cells, registered in a numbered slot of `libktui`'s sprite table, with its
slot and sub-cell position encoded in the cell itself, so the ordinary row comparison already tracks
its redraws. A sprite is always optional: every caller draws a character fallback when none is
available. See [The design
language](../03-architecture/design-language.md#pictures-are-an-enhancement-layer).

### store

`kdos-store`, the window titled "Applications" that lists the *catalogue* and installs what you tick
by running `kdos-appbox install`, which builds it through the store *lane*. It is one of the names
`kdos-shell` answers to. Not to be confused with the pack store, `/var/lib/kdos/packs`, where
installed packs are kept. See [Applications](../02-user-guide/applications.md#the-store).

### surface

One window or popup of the desktop drawn by KDOS: the panel, the Start menu, the control centre and
so on. `kdos-shell` is one binary that answers to 55 names, which reach 54 programs: most of them
surfaces, and two helpers with no surface of their own: the media watcher `kdos-mediad`, whose
toasts `kdos-notifyd` draws, and the filter `kdos-ascii`, which writes text to standard output.
[kdos-shell](../04-programs/kdos-shell.md) uses the word in a wider sense that covers all 54
programs, helpers included. See [kdos-shell](../04-programs/kdos-shell.md).

### target tree

`build/fs`, the root filesystem the build assembles and every phase from `20_selfhost` onwards runs
in as its *chroot*. `70_image` turns it into the ISO. See [The build
system](../05-developer/build-system.md#the-chroot).

### tile

A block of cells drawn as pixels (`kch_tile_*`), for content that cannot be a row of text, such as
the panel's Start button, its meters and the charts in `kdos-res`, of any size up to the grid (one
sprite slot per 16×16 block). A tile owns two sets of slots and alternates between them on every change of content, or the redraw would change no cell and the new content would never be
presented. See [The design
language](../03-architecture/design-language.md#pictures-are-an-enhancement-layer).

### toast

A transient notification popup. `kdos-notifyd` owns `org.freedesktop.Notifications` and shows each
notification as a toast; a toast that expires, is clicked or is closed by its sender joins the
history that `kdos-notify` shows. See [kdos-shell](../04-programs/kdos-shell.md#notifications).

### toybox

The multi-call binary that provides `init` and most everyday commands on the host, and in the
*initramfs*. Where a full tool is needed another port owns the command (util-linux owns `mount`,
`blkid` and `switch_root`, for example) and the toybox build switches that applet off. See [How KDOS
differs](../01-philosophy/how-kdos-differs.md#the-core-userland) and
[Packaging](../03-architecture/packaging.md#toybox-and-the-tools-it-overlaps).

### trial boot

A boot of the candidate *root slot* after an update, before it is confirmed. On UEFI, `kdos-bootctl
try` arms one boot through the firmware's one-shot `BootNext` variable, with a copy of the loader
under `EFI/kdos/trial/` on the *ESP*; without UEFI variables the boot menu leads with the candidate
for a number of boots (default 3). A candidate whose trial is spent without `kdos-bootctl mark-good`
is rolled back. See [Boot and init](../03-architecture/boot-and-init.md#one-boot-through-bootnext).

### vendor bundle

A tarball of a port's language dependencies (Rust crates, Go modules, Python source distributions or
Hackage packages), named `<name>-vendor-<version>.tar.xz`, so that the build needs no network. `make
fetch` takes it from the port directory, the *source cache* or the *source archive* like any other
source, and generates it, by default in the `kdos-fetch` container, only when none of them has it.
It is generated reproducibly, so it has a stable *source hash*. A recipe asks for one with the
`vendoring =` key; 159 ports in `ports/core` set it (71 Rust, 38 Go, 45 Python, 4 Haskell, 1 Node). See
[Writing ports](../05-developer/writing-ports.md#vendoring).

### warmup

Starting the boxes behind your pinned applications in the background at login (`kdos-appbox warmup`,
run by the session at a lowered priority), so the first launcher click does not wait for a container
to start. See [kdos-appbox](../04-programs/kdos-appbox.md#warmup-and-collection).

### whiteout

How a filesystem layer records a deletion. Overlay whiteouts are kept in `trusted.overlay.*`
attributes that only root can write, so `kdos-pack build` runs `mkfs.erofs` as root to preserve
them, and `kdos-appbox export` flattens a box with `podman export` so that no whiteout is involved.
Because `kdos-packd` mounts as root, an application pack can delete a file its base provides. See
[kdos-appbox](../04-programs/kdos-appbox.md#export-and-import) and [Packs and
boxes](../03-architecture/packs-and-boxes.md#composition).

### wlroots

The compositor library that `kdos-comp` is built on (the `wlroots` port, 0.20.2). It provides the
output, input and rendering machinery, including the *scene*. See
[kdos-comp](../04-programs/kdos-comp.md).

### xdg-foreign

The Wayland protocol that lets one client name another client's window by a handle. The portal
backend imports a dialog's parent window through it, so a boxed application's file dialog opens over
the window that asked. See [The session](../03-architecture/session.md#the-kdos-backend).

### Xwayland

An X server that runs as a Wayland client. The compositor runs it rootless, starting it when the
first X11 client connects, so X11-only applications, native or in boxes, work. It serves GLX
through Mesa's `libGLX_mesa` behind `libglvnd`, so an X11 client that draws through GLX gets
OpenGL; Wayland clients draw through EGL. It is the one X server permitted on the host. See [kdos-comp](../04-programs/kdos-comp.md#xwayland) and
[Principles](../01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## Words this book avoids

The book keeps to one vocabulary. These words either blur a distinction the entries above draw or
promise something the text cannot support.

| Not used | Instead |
|---|---|
| "app store" | *The store*, or `kdos-store`: it builds the applications you choose from the catalogue on your machine; there is no account and no purchase |
| "distro" in prose | Distribution |
| "just" | Say the thing without it. "Just run X" hides how much X is |
| "simply", "obviously", "of course" | Omit them; state the fact without judging its difficulty |
| "should work" | Say what was measured, or say it is untested |
| First-person decision narration | State the decision and its reason in the present |
| Past-tense narration of any kind | These pages describe the present. See [Principles](../01-philosophy/principles.md#documentation-describes-the-present) |
| "sandbox" for a box, unqualified | A box limits what an application can do to the desktop, not to your data |
| "group", unqualified, in build prose | *The `group =` key*, or *shelf* where the grouping of the ports tree or a package list is meant |
| "category" for where a port is filed | *Shelf*. "Category" belongs to the catalogue, the pack index and desktop entries |

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
