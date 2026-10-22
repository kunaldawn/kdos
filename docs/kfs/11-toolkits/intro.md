# Toolkits

<!-- kfs:phase 43_toolkits -->

`43_toolkits` builds the widget toolkits that applications are written in, GTK 3 and 4 with
libadwaita, Qt 5 and Qt 6, the KDE Frameworks, wxWidgets, FLTK and Motif, together with the web
engines, portals, services and libraries built over them. It also builds the container runtime that
boxes run on, because of where its dependencies lead. This part describes the rule that fills the
phase, the shelves that carry it, how a toolkit built here chooses between Wayland and X11 when an
application starts, and which of its ports have a full page. The package pages follow, in chapters
by shelf, in the shelves' own order, and within a shelf in the order [kpkgdepends]{.idx} builds them.

## What belongs here

Every file of the phase's list opens with the rule:

<!-- kfs:include script/phases/43_toolkits/packages.d/ai.txt lines=16-18 pin=d75397030850 -->

An application belongs here too when another port depends on it. `konsole`, `vlc`, `audacious` and
`brasero` are programs with windows, but another port names each of them in `depends =`
(`dolphin` names `konsole`, for one), so they are built here and not in [Part XII](../12-apps/intro.md); see
[Which phase lists a port](../../kdos/05-developer/writing-ports.md#which-phase-lists-a-port).

Following the closure rule puts a few system ports here that have nothing graphical about their
purpose. `podman` depends on `gpgme`, which depends on GnuPG, which depends on `pinentry`, the
passphrase prompt, and this tree's `pinentry` is built with its Qt 6 front end; `distrobox`
depends on `podman`. `qemu` depends on `gtk3` and `vte3`, which its GTK display uses. Their pages are in the `containers` and
`virt` chapters of this part.

## The toolkits are for applications

Nothing KDOS draws for itself links a [toolkit]{.idx} from this phase: not the compositor, not the panel,
not the daemons and not the portal backend (see
[Toolkits are for applications](../../kdos/01-philosophy/principles.md#toolkits-are-for-applications-not-the-desktop)).
The rule is checked on the built tree. When a build tree exists, [`testing/preflight.sh`]{.idx term="preflight.sh"} reads the
[`NEEDED`]{.idx term="DT_NEEDED"} entries of every ELF file that a package built from a port under `src/` installs, and
fails on any GTK, GDK, libadwaita, WebKitGTK, Qt, KDE Frameworks, wxWidgets, FLTK or Tk library
among them, and on any X11 client library (`libX…`). `libxcb` is allowed: the compositor speaks
XCB to Xwayland.

Each toolkit that has a Wayland backend is built with it and with its X11 backend, so a program
that has only an X11 path still runs, under [Xwayland]{.idx}. Which one an application uses at run time is
not decided by its recipe. The login profile sets the preference, in
[`fs/etc/profile.d/10-wayland.sh`]{.idx term="/etc/profile.d/10-wayland.sh"}:

<!-- kfs:include fs/etc/profile.d/10-wayland.sh lines=71-83 pin=bdaffc9a9711 -->

What each line means for each toolkit, and how they take the desktop's colours, is in
[Toolkits, Wayland and X11](../../kdos/02-user-guide/applications.md#toolkits-wayland-and-x11);
how the Qt 6 modules are configured to match is in
[How Qt 6 is built](../../kdos/05-developer/writing-ports.md#how-qt-6-is-built).

## The list, by shelf

The list is `script/phases/43_toolkits/packages.d/`, one file per shelf, read in byte order as one
list, with no order file. The chapters that carry the phase are these:

`gtk`
: GTK 3 and GTK 4 with their C++ bindings, libadwaita and libhandy, VTE, GtkSourceView, gvfs,
  `libpeas`, `blueprint-compiler`, and the Xfce libraries. GLib, below them, is Part IX's.

`qt6`, `qt5`, `qt-extra`
: The Qt 6 modules from `qtbase` up to QtWebEngine, with PySide6; the subset of Qt 5 that
  applications build on; and third-party Qt libraries outside KDE Frameworks.

`kf6`, `kde`
: KDE Frameworks 6, from `kcoreaddons` and `kconfig` to KIO and Kirigami, and the KDE pieces
  outside Frameworks that applications use without a Plasma desktop: the Plasma integration
  plugin, KIO workers and thumbnailers, and the KDE Wayland libraries.

`toolkits`, `web-engines`
: wxWidgets, FLTK and Motif; WebKitGTK, built twice, as `webkitgtk` for GTK 3 and `webkitgtk6`
  for GTK 4.

`xdg`
: The GTK portal backend, `libportal`, `libcanberra`, the application-indicator libraries and
  `zenity`.

The subject chapters
: `crypto` and `security` (GPGME and its Qt binding, GnuPG, `pinentry`, `libkleo`), `containers`
  and `virt`, `printing` (cups-filters, SANE and the KDE scanning libraries), `disk` (`udisks2`
  and `libblockdev`), `media-frameworks` (the remaining GStreamer plugin sets and MLT), and the
  other shelves' libraries over a toolkit, each in the chapter of its own shelf.

## Which ports have full pages

The one [Tier A]{.idx} port of this phase is [podman](#port-podman), the runtime under every [box]{.idx} (see
[The three tiers](../00-front/reading-a-package-page.md#the-three-tiers)).

Other ports have [Tier B]{.idx} pages. `qt6-qtbase`, `gtk3`, `gtk4`, `qt6-qtdeclarative`,
`qt6-qtwayland` and the core Frameworks, `kcoreaddons`, `kconfig`, `ki18n` and `kio` among them,
are Tier B because many ports depend on them, and their *Required by* lists are where to look
before changing one. Others are Tier B for a patch or a long `build.sh`. The rest of the phase is
on cards.

## See also

- [The userland: `40_lang` to `44_apps`](../../kdos/05-developer/how-kdos-is-built.md#the-userland-40_lang-to-44_apps): the phase among the five userland phases
- [A graphical application](../../kdos/05-developer/writing-ports.md#a-graphical-application): the recipe shape of a port that builds on these toolkits
- [Writing desktop software](../../kdos/05-developer/writing-desktop-software.md): how KDOS's own programs draw without them
- [Reading a package page](../00-front/reading-a-package-page.md): how the pages below are laid out
- [Part XII](../12-apps/intro.md): the next phase

<!-- book-nav -->
---

*Part XI. Toolkits.* Previous: [Part X. The graphics stack](../10-graphics/intro.md) · [Contents](../README.md) · Next: [Part XII. Applications](../12-apps/intro.md)
