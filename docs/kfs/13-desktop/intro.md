# The desktop

<!-- kfs:phase 50_desktop -->

`50_desktop` builds KDOS's desktop: the display manager [SDDM]{.idx}, the [compositor]{.idx}
KWin, the Plasma shell and its daemons, System Settings, the [portal]{.idx} backend, the
[input method]{.idx} with its language engines, Discover, KDOS's own Plasma code from
`src/desktop`, the root daemons from `src/daemons`, and the meta ports that name the KDE install
set. This part explains why the desktop is a phase of its own and why those two directories are
searched here and nowhere else, then walks through the phase in the order it builds, with a link
to each port's page and to the chapter of *The KDOS book* that describes how the program behaves.
The package pages follow, in chapters by shelf, in the shelves' own order, and within a shelf in
the order [kpkgdepends]{.idx} builds them.

## Why the desktop is a phase of its own

The list's header says what the phase is for and what it leaves to the phases before it:

<!-- kfs:include script/phases/50_desktop/packages.txt lines=13-18 pin=f578e095ae03 -->

The Qt and KDE Frameworks libraries, and the Plasma libraries that applications link, therefore
have their pages in [Part XI](../11-toolkits/intro.md), and Wayland's own libraries in
[Part IX](../09-system/intro.md) and [Part X](../10-graphics/intro.md). What is left for this
phase is what only the session needs, and the phase table in
[Which phase lists a port](../../kdos/05-developer/writing-ports.md#which-phase-lists-a-port)
gives exactly that as its rule. fcitx5's dependencies are all installed by the end of Part XI, so
by closure alone it could be built there; it is listed here because it is the program on the
other end of KWin's input-method protocol, and the list's comment above it says so.

The programs from `src/` have a stronger reason. The phase's [`phase.env`]{.idx} is the only one whose
[`PORT_REPO`]{.idx} names `src/desktop` and `src/daemons`:

<!-- kfs:include script/phases/50_desktop/phase.env lines=25-29 pin=7ed612b2a98f -->

kpkg looks for a port only in the repositories `PORT_REPO` names (see
[Where kpkg looks for a port](../../kdos/05-developer/build-system.md#where-kpkg-looks-for-a-port)),
so no earlier phase can build these programs, and [`testing/phaseclosure.py`]{.idx term="phaseclosure.py"} refuses a port in an
earlier phase that depends on one of them, since that dependency could only be resolved by a later
phase's `PORT_REPO`. KDOS's settings pages, widgets, KWin plugins and Discover backend link
libplasma, KWin and libdiscover, which this phase installs, and the daemons are built beside them.

The split also gives the build a restore point. The [snapshot]{.idx} taken at the end of `44_apps` holds
the whole userland and none of the desktop, so resuming from it re-runs only this phase, the kernel
and the image (see
[The desktop: `50_desktop`](../../kdos/05-developer/how-kdos-is-built.md#the-desktop-50_desktop)).

## The list and its chapters

The list is one file, `script/phases/50_desktop/packages.txt`. A heading of the form
`# <shelf> — <description>` opens each shelf's ports: `auth`, `sysmon`, `hardware`, `input`,
`disk`, `filesystem`, `security`, `transfer`, `graphics`, `xdg`, `kde` and `printing` for the
upstream ports, then `src-art`, `src-desktop` and `src-daemons` for KDOS's own, and `meta` last
for the ports that only name others. No name comes before the first heading, so the list has no
[pinned run]{.idx} (see
[Package lists](../../kdos/05-developer/build-system.md#package-lists)), and under
[`kdosbuild --port-jobs`]{.idx term="--port-jobs"} no [serial prefix]{.idx} (see
[Building a package phase by level](../../kdos/05-developer/build-system.md#building-a-package-phase-by-level)).
The chapters below follow the ports' own shelves, with the `src-` chapters after the upstream
ones; a meta port sits on the `kde` shelf.

## A tour of the build

kpkgdepends builds the phase in the order the list gives, each port after what it depends on. Read
in that order, the phase falls into six groups.

### The login screen

[sddm](#port-sddm) is the display manager. It is built for Qt 6 with elogind and no systemd, and
installs three PAM stacks of KDOS's own, `sddm`, `sddm-autologin` and `sddm-greeter`, in place of
upstream's; the page explains each line. [kwallet-pam](#port-kwallet-pam) opens the user's wallet
with the password typed at the greeter. The greeter's theme, [kdos-sddm-theme](#port-kdos-sddm-theme),
is in the `src-art` group below. How SDDM is started and when it gives way to a text prompt is in
[Login and autologin](../16-wiki/login-and-autologin.md).

### The input method

[fcitx5](#port-fcitx5) comes with its engines: [libime](#port-libime) and
[fcitx5-chinese-addons](#port-fcitx5-chinese-addons) for Chinese,
[anthy-unicode](#port-anthy-unicode) and [fcitx5-anthy](#port-fcitx5-anthy) for Japanese, and
[libhangul](#port-libhangul) and [fcitx5-hangul](#port-fcitx5-hangul) for Korean, then
[fcitx5-qt](#port-fcitx5-qt), [fcitx5-gtk](#port-fcitx5-gtk) and
[fcitx5-configtool](#port-fcitx5-configtool).
[plasma-keyboard](#port-plasma-keyboard) is the on-screen keyboard. fcitx5 is built for Wayland
only, with no X11 frontend and no systemd. Its build would download data at build time, which the
chroot cannot, so the recipe declares the data archive as a second source and `build.sh` puts it
where the download would have written it; libime and the Chinese engine carry their dictionaries
the same way. How KWin relays the input method to applications is in
[Input methods](../../kdos/03-architecture/session.md#input-methods).

### KWin and Plasma

The `kde` section is the session itself. [kwin](#port-kwin) is the compositor and window manager,
with Xwayland for X11 clients; [kglobalacceld](#port-kglobalacceld),
[kscreenlocker](#port-kscreenlocker), [aurorae](#port-aurorae), [knighttime](#port-knighttime)
and [kscreen](#port-kscreen) are the parts it loads or serves. KWin carries KDOS patches, each with
its `Subject:` line on the kwin page. [plasma-workspace](#port-plasma-workspace) is `plasmashell`,
the session's start and end, KRunner and the notifications, and
[plasma-desktop](#port-plasma-desktop) the panel and desktop containments and Kickoff;
[systemsettings](#port-systemsettings) hosts every settings page. The rest of the section is the
session's services and applets: [powerdevil](#port-powerdevil), [plasma-nm](#port-plasma-nm),
[plasma-pa](#port-plasma-pa), [bluedevil](#port-bluedevil),
[polkit-kde-agent-1](#port-polkit-kde-agent-1), [kde-gtk-config](#port-kde-gtk-config),
[kdeplasma-addons](#port-kdeplasma-addons), [discover](#port-discover),
[sddm-kcm](#port-sddm-kcm), [plasma-setup](#port-plasma-setup),
[plasma-welcome](#port-plasma-welcome) and [krohnkite](#port-krohnkite), the auto-tiling script.
Every one of them is built with its systemd parts turned off or removed, and the classic session
start is the only one; the pages say how. How the session starts is in
[The session](../../kdos/03-architecture/session.md).

The sections before `kde` hold the Plasma pieces that belong to a subject shelf: the system
monitor ([ksystemstats](#port-ksystemstats), [plasma-systemmonitor](#port-plasma-systemmonitor),
[kinfocenter](#port-kinfocenter)), [plasma-thunderbolt](#port-plasma-thunderbolt),
[plasma-disks](#port-plasma-disks), [plasma-vault](#port-plasma-vault),
[ksshaskpass](#port-ksshaskpass), the backup tool [kup](#port-kup), [spectacle](#port-spectacle)
and, under `xdg`, [xdg-desktop-portal-kde](#port-xdg-desktop-portal-kde), the portal backend.
`xdg-desktop-portal` itself was built in Part X. [print-manager](#port-print-manager) closes the
upstream ports.

### KDOS's own artwork and Plasma code

The `src-art` and `src-desktop` sections are KDOS's own. They are Qt 6 and KDE Frameworks code,
QML and generated data, each with an ECM `build.sh` or a short install, and each page shows it:

- the look: [kdos-plasma-look](#port-kdos-plasma-look) (the eight colour schemes and global
  themes, the `kdos` Plasma style, the three desks), [kdos-sddm-theme](#port-kdos-sddm-theme) and
  [kdos-addons](#port-kdos-addons), the offline add-on set;
- System Settings pages: [kcm-kdos-system](#port-kcm-kdos-system) (the two root slots),
  [kcm-kdos-boxes](#port-kcm-kdos-boxes), [kcm-kdos-firewall](#port-kcm-kdos-firewall),
  [kcm-kdos-look](#port-kcm-kdos-look) and [kcm-kdos-health](#port-kcm-kdos-health);
- KWin plugins: [kdos-kwin-motion](#port-kdos-kwin-motion), [kwin-effect-kdos-keys](#port-kwin-effect-kdos-keys),
  [kwin-script-kdos-runraise](#port-kwin-script-kdos-runraise) and
  [kwin-script-kdos-window-memory](#port-kwin-script-kdos-window-memory);
- the shell and its services: [kdos-welcome](#port-kdos-welcome),
  [plasmoid-kdos-checklist](#port-plasmoid-kdos-checklist),
  [plasmoid-kdos-privacy](#port-plasmoid-kdos-privacy), [kded-kdos](#port-kded-kdos),
  [kdos-runner](#port-kdos-runner), [kdos-notice](#port-kdos-notice),
  [ksystemstats-kdos](#port-ksystemstats-kdos), [kio-library](#port-kio-library) and
  [kdos-transcribe](#port-kdos-transcribe);
- [kdos-discover](#port-kdos-discover), the Discover backend for packs, boxes and host ports and
  the system-update notifier;
- [kdos-installer-gui](#port-kdos-installer-gui), the graphical installer over the same backend
  as the terminal one.

What each does once it runs is in *The KDOS book*: [The Plasma shell](../../kdos/04-programs/plasma-shell.md),
[KWin](../../kdos/04-programs/kwin.md), [KDOS's settings](../../kdos/04-programs/kdos-settings.md)
and [Discover](../../kdos/04-programs/discover.md).

### The root daemons

The `src-daemons` section is the root daemons: [kdos-powerd](#port-kdos-powerd),
[kdos-energyd](#port-kdos-energyd), [kdos-oomd](#port-kdos-oomd),
[kdos-packd](#port-kdos-packd), [kdos-settingsd](#port-kdos-settingsd) and
[kdos-sysupdated](#port-kdos-sysupdated). Each is a small C program compiled with `libkbase` and
whatever else it needs, installed in `/usr/sbin`, and those with a command-line client install it
as a link in `/usr/bin` to the same binary. They share one shape, a root process under
[`ksvc supervise`]{.idx term="supervisor (ksvc)"} that owns a socket in `/run` or a name on the
system bus and authorises each request by the caller's credentials or through polkit, described in
[The shape they share](../../kdos/04-programs/daemons.md#the-shape-they-share).

### The meta ports

The `meta` section closes the phase: [kde-plasma](#port-kde-plasma), the session, and the Gear
sets [kde-gear-core](#port-kde-gear-core), [kde-gear-games](#port-kde-gear-games),
[kde-gear-education](#port-kde-gear-education), [kde-gear-pim](#port-kde-gear-pim),
[kde-gear-multimedia](#port-kde-gear-multimedia), [kde-gear-graphics](#port-kde-gear-graphics),
[kde-gear-utilities](#port-kde-gear-utilities) and [kde-gear-devtools](#port-kde-gear-devtools),
then [kde-gear](#port-kde-gear) and [kde-all](#port-kde-all). A meta port has no source and
installs no file; it only names other ports in `depends`, so `kpkg install kde-all` gives a
machine the whole desktop. Gear is split by kind so no one recipe names more than kpkg's 128
dependencies.

## Changing a desktop program

Every port under `src/` that has no `source =` counts the whole of `src/libs` as part of its
[recipe]{.idx term="recipe hash"}, so an edit to a library makes every one of them out of date, in this phase and in the
userland phases that build KDOS's own programs (see
[The recipes under src/](../../kdos/03-architecture/packaging.md#the-recipes-under-src)). The build
plan for one program, and the faster loop that puts a
rebuilt program into a running image without packaging a new one, are in
[Rebuilding one thing](../../kdos/05-developer/developing.md#rebuilding-one-thing).

## Which ports have full pages

The ports of KDOS's own and the core of the session are [Tier A]{.idx}: `docs/kfs/data/tier-a.txt`
names them, and each page carries the hand-written note beside its recipe (see
[The three tiers](../00-front/reading-a-package-page.md#the-three-tiers)). The other ports of the
phase are Tier B or C by the general rule.

## See also

- [The programs](../../kdos/04-programs/README.md): every program of KDOS's own and where it is documented
- [The session](../../kdos/03-architecture/session.md): how the programs built here start and fit together
- [Writing desktop software](../../kdos/05-developer/writing-desktop-software.md): how KDOS's Plasma code is written
- [Reading a package page](../00-front/reading-a-package-page.md): how the pages below are laid out
- [Part XIV](../14-kernel/intro.md): the next phase

<!-- book-nav -->
---

*Part XIII. The desktop.* Previous: [Part XII. Applications](../12-apps/intro.md) · [Contents](../README.md) · Next: [Part XIV. The kernel](../14-kernel/intro.md)
