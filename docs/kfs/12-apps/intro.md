# Applications

<!-- kfs:phase 44_apps -->

`44_apps` is the last of the userland phases. It builds the natively ported applications, the ports
at the top of the dependency graph that nothing else needs: browsers, office suites, the graphics,
audio and video programs, games and emulators, the science, engineering and radio applications,
and the box launcher `kdos-appbox`. Each is built with the toolkit its upstream uses, from
[Part XI](../11-toolkits/intro.md). This part describes the rule that fills the phase, why some
programs without a window of their own are built in it, how the shelves divide it, and which of its
ports have a full page. The package pages follow, in chapters by shelf, in the shelves' own order,
and within a shelf in the order [kpkgdepends]{.idx} builds them.

## What belongs here

Every file of the phase's list opens with the rule:

<!-- kfs:include script/phases/44_apps/packages.d/3d.txt lines=16-17 pin=5b0d56824dd0 -->

Every port here is a leaf, with nothing in the tree depending on it. An application another port
depends on is listed in an earlier phase instead: in Part XI when its closure reaches a toolkit,
as `konsole`, `vlc` and `gpsman` (which `xastir` uses) are, and in [Part X](../10-graphics/intro.md)
when it reaches only the graphics stack. A library an application here links sits where its own
closure puts it: `libdovi`, which `handbrake` uses, is in Part IX. [`testing/phaseclosure.py`]{.idx term="phaseclosure.py"} refuses a port of this phase that
something depends on or whose closure does not qualify it. The full table of phases is under
[Which phase lists a port](../../kdos/05-developer/writing-ports.md#which-phase-lists-a-port).

The rule is about the [closure]{.idx term="dependency closure"}, not about having a window. Several terminal programs are here
because something they depend on reaches a toolkit: `buildah`, `skopeo` and the XMPP client
`profanity` depend on `gpgme`, whose GnuPG asks for passphrases through a `pinentry` built against
Qt 6; `aerc` depends on `notmuch`, `yazi` on `poppler` and `sc-im` on `gnuplot`, all three Part XI
ports.
The *Depends on* list of each page shows the link.

The reverse also holds. An application whose closure stays clear of the graphics stack is a
[Part IX](../09-system/intro.md) port whatever its shelf, as Neovim, `nethack` and `lynx` are,
and a port on an application shelf whose closure reaches the graphics stack but no toolkit is
built here only because of that shelf.

## The list, by shelf

The list is `script/phases/44_apps/packages.d/`, one file per shelf, read in byte order as one list,
with no order file (see
[Package lists](../../kdos/05-developer/build-system.md#package-lists)). Most of the chapters fall
into a few groups:

Making and viewing
: `graphics` (GIMP, Krita, Inkscape, darktable, digiKam and the image viewers), `3d`, `cad`,
  `fabrication`, `documents` and `office` (LibreOffice, Gnumeric, GnuCash).

Sound and pictures in motion
: `studio` (Ardour, Audacity, MuseScore, LMMS and the trackers), `music-players`,
  `video-players` and `video-tools`.

Play
: the `games-*` shelves, `emulators` (Wine, ScummVM, DOSBox and the console emulators) and
  `libretro`.

The network
: `browsers` (Firefox ESR, Chromium, LibreWolf, Falkon and others), `mail`, `chat` and
  `transfer`.

Work and study
: `editors` (Kate, Qt Creator, KDevelop, JupyterLab), `education`, `math`, `astronomy`, `gis`,
  `eda`, `sdr` and `hamradio`.

The system's own tools
: `files`, `disk`, `sysmon`, `containers`, `virt`, `security` and `printing`, where the graphical
  front ends to Part IX's machinery are: Dolphin, GParted, virt-manager and the like.

The other shelves
: `accessibility`, `pim`, `optical`, `vcs`, `themes` and the rest hold a few ports each:
  applications, and the fonts, themes and libraries that nothing else depends on.

`src-system`
: [kdos-appbox](#port-kdos-appbox), which starts and manages [boxed applications]{.idx term="box"} and writes their
  launchers. It depends on `podman` and `distrobox`, which puts it in this phase; see
  [kdos-appbox](../../kdos/04-programs/kdos-appbox.md).

## What every application recipe shares

The shape of an application's recipe, and the habits recurring through this phase's recipes, such
as naming every optional dependency rather than leaving it to whatever happens to be installed, are
in [A graphical application](../../kdos/05-developer/writing-ports.md#a-graphical-application). An
application's [desktop entry]{.idx} is installed by its own port, which is what gives it a row in the [Start
menu]{.idx term="Start menu"}; see
[Desktop entries belong to the port](../../kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port).

A recipe here chooses which display backends its program is built with, where upstream offers a
choice; it does not choose which one runs. The toolkits are built with both backends, and the
login profile makes Wayland the first choice, as
[Part XI](../11-toolkits/intro.md#the-toolkits-are-for-applications) shows. A program with no
[Wayland]{.idx} path at all runs under [Xwayland]{.idx} with no change to its recipe.

## Which ports have full pages

The one [Tier A]{.idx} port of this phase is `kdos-appbox`, as every program of KDOS's own is (see
[The three tiers](../00-front/reading-a-package-page.md#the-three-tiers)).

A large share of the others are [Tier B]{.idx}, and here the reasons are a `build.sh` longer than the
threshold, a patch, or a vendor bundle; none is Tier B for having many dependants, because by the
phase's rule almost nothing depends on an application. The rest are on cards.

## See also

- [Applications](../../kdos/02-user-guide/applications.md): the native applications by need, and how they behave once installed
- [The userland: `40_lang` to `44_apps`](../../kdos/05-developer/how-kdos-is-built.md#the-userland-40_lang-to-44_apps): the phase among the five userland phases
- [Reading a package page](../00-front/reading-a-package-page.md): how the pages below are laid out
- [Part XIII](../13-desktop/intro.md): the next phase

<!-- book-nav -->
---

*Part XII. Applications.* Previous: [Part XI. Toolkits](../11-toolkits/intro.md) · [Contents](../README.md) · Next: [Part XIII. The desktop](../13-desktop/intro.md)
