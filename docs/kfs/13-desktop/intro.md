# The desktop

<!-- kfs:phase 50_desktop -->

`50_desktop` builds KDOS's desktop: wlroots, and on it the [compositor]{.idx} `kdos-comp`, the [panel]{.idx}
`kdos-shell`, the terminal, the [lock screen]{.idx}, the resource monitor, the per-box Wayland socket, the
screen recorder, the portal backends, the [input method]{.idx} with its language engines, and the root
daemons the desktop account talks to. Most of its ports are programs of KDOS's own, from
`src/desktop` and `src/daemons`. This part explains why the desktop is a phase of its own and why
those two directories are searched here and nowhere else, then walks through the phase in the order
it builds, from the input method and wlroots to the programs, with a link to each port's page and
to the chapter of *The KDOS book* that describes how the program behaves. The package pages follow,
in chapters by shelf, in the shelves' own order, and within a shelf in the order [kpkgdepends]{.idx} builds
them.

## Why the desktop is a phase of its own

The list's header says what the phase is for and what it leaves to the phases before it:

<!-- kfs:include script/phases/50_desktop/packages.txt lines=13-18 pin=f925a6ec8293 -->

The Wayland base therefore has its pages in [Part IX](../09-system/intro.md) and
[Part X](../10-graphics/intro.md). What is left for this phase is what only the desktop needs, and
the phase table in
[Which phase lists a port](../../kdos/05-developer/writing-ports.md#which-phase-lists-a-port)
gives exactly that as its rule. fcitx5's dependencies are all installed by the end of
[Part XI](../11-toolkits/intro.md), where `plasma-wayland-protocols` is built, so by closure alone
it could be built there. It is listed here under the second half of that rule: it is the program
on the other end of the compositor's input-method protocol, and the list's comment above it says
so.

The programs from `src/` have a stronger reason. The phase's [`phase.env`]{.idx} is the only one whose
[`PORT_REPO`]{.idx} names `src/desktop` and `src/daemons`:

<!-- kfs:include script/phases/50_desktop/phase.env lines=25-28 pin=e2f92d9382dc -->

kpkg looks for a port only in the repositories `PORT_REPO` names (see
[Where kpkg looks for a port](../../kdos/05-developer/build-system.md#where-kpkg-looks-for-a-port)),
so no earlier phase can build these programs, and [`testing/phaseclosure.py`]{.idx term="phaseclosure.py"} refuses a port in an
earlier phase that depends on one of them, since that dependency could only be resolved by a later
phase's `PORT_REPO`. The compositor and the programs that speak wlroots' protocols need wlroots,
which this phase installs, and the daemons are built beside them.

The split also gives the build a restore point. The [snapshot]{.idx} taken at the end of `44_apps` holds
the whole userland and none of the desktop, so resuming from it re-runs only this phase, the kernel
and the image (see
[The desktop: `50_desktop`](../../kdos/05-developer/how-kdos-is-built.md#the-desktop-50_desktop)).

## The list and its chapters

The list is one file, `script/phases/50_desktop/packages.txt`. A heading of the form
`# <shelf> — <description>` opens each shelf's ports: `input`, `wl` and `xdg` for the upstream
ports, then `src-desktop` and `src-daemons` for KDOS's own. No name comes before the first heading,
so the list has no [pinned run]{.idx} (see
[Package lists](../../kdos/05-developer/build-system.md#package-lists)), and under
[`kdosbuild --port-jobs`]{.idx term="--port-jobs"} no [serial prefix]{.idx} (see
[Building a package phase by level](../../kdos/05-developer/build-system.md#building-a-package-phase-by-level)). The chapters
below follow the same shelves, with the two `src-` chapters after the upstream ones, `src-daemons`
first.

## A tour of the build

kpkgdepends builds the phase in the order the list gives, each port after what it depends on. Read
in that order, the phase falls into five groups. The one port out of place below is the KDOS portal
backend, described with the other portal: it depends on `kdos-shell` and builds after it.

### The input method

[fcitx5](#port-fcitx5) is first, with its engines after it: [libime](#port-libime) and
[fcitx5-chinese-addons](#port-fcitx5-chinese-addons) for Chinese,
[anthy-unicode](#port-anthy-unicode) and [fcitx5-anthy](#port-fcitx5-anthy) for Japanese, and
[libhangul](#port-libhangul) and [fcitx5-hangul](#port-fcitx5-hangul) for Korean. fcitx5 is built
for Wayland only, with no X11 frontend and no systemd. Its build would download data at build
time, which the chroot cannot, so the recipe declares the data archive as a second source and
`build.sh` puts it where the download would have written it; libime and the Chinese engine carry
their dictionaries the same way. The fcitx5 page explains each switch. How the compositor relays
the input method to applications is in
[Input methods](../../kdos/03-architecture/session.md#input-methods).

### wlroots and its patch

[wlroots]{.idx} is the library the compositor is built from. Its whole `build.sh` is short:

<!-- kfs:include ports/core/wl/wlroots/build.sh lines=12-32 pin=81802afd47c0 -->

kpkg applies no patch by itself, so the first line does it. `idle-capture-frame.patch` changes
wlroots' `ext-image-copy-capture` implementation so that each capture request asks for a new frame
even when nothing on the screen has changed; without it, a recording of a still screen receives one
frame and then nothing, and the patch's own header gives the cost. The `meson` switches choose the
DRM and libinput backends, the GLES2 and Vulkan renderers, the allocators, session, Xwayland and
colour-management support; the [wlroots](#port-wlroots) page explains each one.

The last two lines are KDOS's addition. They install the protocol XML files from the tarball into
[`/usr/share/wlroots/protocols/`]{.idx term="/usr/share/wlroots/protocols"}, where `kdos-shell`, `kdos-term`, `kdos-res` and `kdos-lock` read
the layer-shell and foreign-toplevel-management protocols, and `kdos-shell` output management and
data control as well, to generate their client code. Those programs depend on
wlroots for that directory and link none of it; `kdos-comp` is the one port that links the library.
wlroots changes its API from one release series to the next, so a version bump of this port is a
change to `kdos-comp` as well.

### The portals

[`xdg-desktop-portal`]{.idx} itself was built in Part X. This phase adds its two backends.
[xdg-desktop-portal-wlr](#port-xdg-desktop-portal-wlr) implements ScreenCast and Screenshot by
capturing through the compositor's capture protocols and handing the frames to PipeWire.
`xdg-desktop-portal` has no capture code of its own, so a recorder or a browser, native or boxed,
that asks the [portal]{.idx} for the screen is served by this backend.
[xdg-desktop-portal-kdos](#port-xdg-desktop-portal-kdos) implements FileChooser, Settings,
AppChooser and Access. It is one C file compiled against [`basu`]{.idx}, installed with its `.portal` file
and its D-Bus service file, and it depends on `kdos-shell` because it answers a file-chooser
request by running [`kdos-pick`]{.idx} and an access question by running [`kdos-prompt`]{.idx}, two of the shell's
names. How the session chooses between the backends is in
[Portals](../../kdos/03-architecture/session.md#portals).

### The compositor and the programs

[kdos-comp](#port-kdos-comp) is a hard fork of [labwc]{.idx}, kept in the tree under
`src/desktop/kdos-comp` with its upstream licence; the fork's [`KDOS-FORK`]{.idx} file names the labwc
release and tarball it started from. Its `build.sh` shows the step that most programs of KDOS's
own have in common, compiling the libraries in:

<!-- kfs:include src/desktop/kdos-comp/build.sh lines=12-25 pin=09a7acfcdeca -->

The first block compiles three of KDOS's libraries, [`libkbase`]{.idx}, [`libkcolor`]{.idx} and [`libkwm`]{.idx}, straight
from [`src/libs`]{.idx} into a static archive in the build directory, and passes the archive on through
`LDFLAGS`. The libraries are not packages of their own: each program compiles in the ones it uses
(see [The C libraries](../../kdos/05-developer/c-libraries.md)). The `meson` switches turn on
Xwayland support and turn off labwc's window icons, SVG window buttons, native language support,
manual pages, the `labnag` notification daemon and the systemd user unit.

The other programs have no build system of their own: each `build.sh` is a set of compiler lines
over the program's files and the libraries it needs, and each page shows it whole:

- [kdos-boxsock](#port-kdos-boxsock), the Wayland socket a box's applications connect through,
  tagged by the compositor with the box's name. It links only `wayland-client`.
- [kdos-shell](#port-kdos-shell), the panel and every other surface, one binary installed under
  many names. Its `build.sh`, the longest of the phase, generates the client code of every
  protocol the shell speaks before it compiles.
- [kdos-term](#port-kdos-term), the terminal, with the picture protocols compiled in.
- [kdos-lock](#port-kdos-lock), the lock screen, and with it [`kdos-checkpass`]{.idx}, installed [setuid]{.idx}
  root.
- [kdos-res](#port-kdos-res), the resource monitor, and with it [`kdos-resctl`]{.idx}, the other setuid-root
  helper.
- [kdos-record](#port-kdos-record), the screen recorder, which depends on
  xdg-desktop-portal-wlr and the GStreamer plugin sets.

What each program does once it runs is in *The KDOS book*:
[kdos-comp](../../kdos/04-programs/kdos-comp.md), [kdos-shell](../../kdos/04-programs/kdos-shell.md),
[kdos-term](../../kdos/04-programs/kdos-term.md), [kdos-res](../../kdos/04-programs/kdos-res.md),
[kdos-lock](../../kdos/04-programs/daemons.md#kdos-lock),
[kdos-boxsock](../../kdos/04-programs/daemons.md#kdos-boxsock),
[xdg-desktop-portal-kdos](../../kdos/04-programs/daemons.md#xdg-desktop-portal-kdos) and, for
kdos-record, [Recording](../../kdos/03-architecture/session.md#recording).

### The root daemons

The phase ends with the root daemons: [kdos-powerd](#port-kdos-powerd),
[kdos-energyd](#port-kdos-energyd), [kdos-oomd](#port-kdos-oomd),
[kdos-mountd](#port-kdos-mountd) and [kdos-packd](#port-kdos-packd). Each is a small C program
compiled with `libkbase` and whatever else it needs, installed in `/usr/sbin`, and those with a
command-line client install it as a link in `/usr/bin` to the same binary. They share one shape,
a root process under [`ksvc supervise`]{.idx term="supervisor (ksvc)"} that owns one socket in `/run` and authorises each request by
the caller's credentials, described in
[The shape they share](../../kdos/04-programs/daemons.md#the-shape-they-share).

## Changing a desktop program

Every port under `src/` that has no `source =` counts the whole of `src/libs` as part of its
[recipe]{.idx term="recipe hash"}, so an edit to a library makes every one of them out of date, in this phase and in the
userland phases that build KDOS's own programs (see
[The recipes under src/](../../kdos/03-architecture/packaging.md#the-recipes-under-src)). The build
plan for one program, and the faster loop that puts a
rebuilt program into a running image without packaging a new one, are in
[Rebuilding one thing](../../kdos/05-developer/developing.md#rebuilding-one-thing).

## Which ports have full pages

Every port of this phase is [Tier A]{.idx}: `docs/kfs/data/tier-a.txt` names the phase's whole list, and
each page carries the hand-written note beside its recipe (see
[The three tiers](../00-front/reading-a-package-page.md#the-three-tiers)).

## See also

- [The programs](../../kdos/04-programs/README.md#the-desktop): every desktop program and where it is documented
- [The session](../../kdos/03-architecture/session.md): how the programs built here start and fit together
- [Writing desktop software](../../kdos/05-developer/writing-desktop-software.md): how a program of this phase draws
- [The C libraries](../../kdos/05-developer/c-libraries.md): the `libk*` libraries the programs compile in
- [Reading a package page](../00-front/reading-a-package-page.md): how the pages below are laid out
- [Part XIV](../14-kernel/intro.md): the next phase

<!-- book-nav -->
---

*Part XIII. The desktop.* Previous: [Part XII. Applications](../12-apps/intro.md) · [Contents](../README.md) · Next: [Part XIV. The kernel](../14-kernel/intro.md)
