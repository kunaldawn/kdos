# Writing desktop software

KDOS's desktop is KDE Plasma, and the code KDOS writes for it is ordinary Plasma code: System
Settings modules, Plasma widgets, a KRunner runner, KWin effects and scripts, a kded module, a
Discover backend, Welcome Center pages and the graphical installer, written in C++ against Qt 6 and
KDE Frameworks 6, in QML with Kirigami, and in JavaScript where KWin takes it. This chapter is for
developers who add such a piece or change one. It explains where the code lives and how a port of
it is built, the names every piece carries, the toolkit rule that keeps Qt out of the rest of the
tree, each kind of piece and the shape KDOS gives it, the conventions for colour, strings, layout,
accessibility, starting programs and reaching the root daemons, and how a change is tested and
tried on a running desktop.

Read three things first:

- [The design language](../03-architecture/design-language.md). It says how a KDOS piece looks:
  the accents and the colour roles they become, the fonts, motion and Phosphor mode.
- The Part IV chapter for the kind of piece you are writing. Each piece's behaviour is documented
  there, not here: the settings modules and the System Monitor sensors in
  [System Settings](../04-programs/kdos-settings.md); the runner, the Welcome pages and the widgets
  in [The Plasma shell](../04-programs/plasma-shell.md); the Discover backend, the update notifier
  and `kdos-sysupdated` in [Discover](../04-programs/discover.md); KWin's patches, effects and
  scripts in [KWin](../04-programs/kwin.md).
- [Writing ports](writing-ports.md). Every piece is a port, built and packaged like any other.

A finished piece builds from its own port, passes its tests in the development image, names every
control for a screen reader, takes every colour from the theme, and has its behaviour written in
the book.

## What KDOS writes for Plasma

Every piece is a port under `src/desktop/<name>/`. Most are Qt; two are C programs that speak to
Plasma over D-Bus and link no Qt at all.

| Port | Kind | What it is |
|---|---|---|
| `kcm-kdos-system` | System Settings module | The A/B system slots: which version starts, its trial, where updates come from |
| `kcm-kdos-boxes` | System Settings module | Per-box permissions, grants, colour and lifecycle, over `kdos-box` |
| `kcm-kdos-firewall` | System Settings module | The firewall's named services through `kdos-powerd`, and the prompt that offers to let a blocked program in |
| `kcm-kdos-look` | System Settings module | One accent everywhere, and Phosphor mode |
| `kcm-kdos-health` | System Settings module | The Health page of Info Center and System Settings, and the KDOS rows of About this System |
| `ksystemstats-kdos` | ksystemstats plugin, sensor face and page | System Monitor's KDOS sensors: boxes, energy by application, the A/B slots |
| `plasmoid-kdos-privacy` | Plasma widget | Microphone, camera and screen lamps in the tray, naming the program and its box |
| `plasmoid-kdos-checklist` | Plasma widget and QML module | The first-run checklist, ticking itself as each step is done |
| `kdos-runner` | KRunner D-Bus runner, in C | The `kdos` verbs, favourites, routes, packs on the boot medium, shortcuts and the offline library in KRunner |
| `kdos-notice` | Session program, in C | The system's notices: restarts after an update, CVEs, the trial slot, a rollback, memory kills; confirms a booted slot |
| `kded-kdos` | kded6 module | Keeps the accent in step with Plasma, offers the sets it finds on a stick, and starts Samba for a login that can share folders |
| `kdos-discover` | Discover backend and notifier plugin | Packs, boxes and ports in Discover, and the A/B update notifier |
| `kio-library` | KIO worker | `library:/`, the offline library as a folder in Dolphin and the file dialogs |
| `kdos-transcribe` | KIO file-item action plugin | Transcribe in Dolphin's menu for audio and video, offline |
| `kdos-kwin-motion` | KWin effects (JavaScript) | Windows unfold and fold, the splash powers on, logout powers down |
| `kwin-effect-kdos-keys` | KWin effect (QML) | The key card on `Meta+F1` and `Meta+/` |
| `kwin-script-kdos-runraise` | KWin script | One key per terminal program raises its Konsole window or opens one |
| `kwin-script-kdos-window-memory` | KWin script | A program's window opens where its last one closed |
| `kdos-plasma-look` | Plasma packages, generated | The eight global themes, the `kdos` Plasma style, the three desks, KSplash, and the defaults under `/usr/share/kdos/xdg` |
| `kdos-welcome` | Welcome Center pages | The KDOS tour: accessibility, accent, Phosphor mode, the desk picker, system and help |
| `kdos-installer-gui` | Kirigami application | The graphical installer, a front end over `kinstall` |

The full-screen CRT effect of Phosphor mode is not among them: it is a built-in KWin effect added
by a patch to the `kwin` port, because KWin refuses an out-of-tree compiled effect built for
another point release. See [KWin](../04-programs/kwin.md).

## Where the code lives

A piece is a port of KDOS's own (see
[A port of KDOS's own](writing-ports.md#a-port-of-kdoss-own)): `kpkgbuild` with no `source =`,
`build.sh`, `kfs.md` and the sources, all in `src/desktop/<name>/`. Its recipe is named in the
`src-desktop` section of `script/phases/50_desktop/packages.txt`. `50_desktop` is the only phase
whose `PORT_REPO` includes `src/desktop` and `src/daemons`, and the phase that installs KWin,
`plasma-workspace`, Discover and the rest of the session these pieces build against. No port from
`41_system` to `44_apps` may depend on a `50_desktop` port, so a piece is never a dependency of an
application.

Code that is not part of the session lives elsewhere, even when a piece uses it: box and system
infrastructure written in C (`kdos-boxsock`, `kinstall-core`) under `src/system`, root daemons
(`kdos-sysupdated`, `kdos-settingsd`) under `src/daemons`, and generated artwork (`kdos-theme`,
`kdos-wallpapers`, `kdos-sddm-theme`) under `src/art`.

### The build.sh

A Qt piece's `build.sh` is the CMake shape every KDE port uses, configured from `$PORT_SRC`,
because the port has no tarball and the build tree is made in the empty `$SRC`:

```bash
cmake -S "$PORT_SRC" -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
```

`KDE_INSTALL_USE_QT_SYS_PATHS=ON` puts plugins and QML modules in Qt's own directories, where
Plasma looks for them; the flags are explained in
[Writing ports](writing-ports.md#kde-frameworks-plasma-and-gear). `BUILD_TESTING=OFF` keeps the
tests out of the package, and the test runner turns it on (see [Testing a piece](#testing-a-piece)).
A piece with a C helper compiles it after the CMake build with `gcc`, against `src/libs`, as any C
program of KDOS's own does: `kdos-discover` builds `kdos-ports-helper`, and from the same source
with `-DKPH_BUILD` `kdos-ports-builder`, that way (see
[The C libraries](c-libraries.md#how-the-libraries-are-built)).

### The CMakeLists.txt

Every Qt piece starts the same way: find ECM, include `KDEInstallDirs`, `KDECMakeSettings` and
`KDECompilerSettings`, find the Qt 6 and KF6 components it uses at stated minimum versions, define
its translation domain, and end with `feature_summary(... FATAL_ON_MISSING_REQUIRED_PACKAGES)`, so a
missing component stops the configure instead of dropping a feature. The logic a test needs goes
into a static library of its own, linked by both the plugin and the tests, because a test cannot
load a plugin into System Settings or plasmashell. From `kcm-kdos-look`:

```cmake
add_definitions(-DTRANSLATION_DOMAIN=\"kdos-kcm-look\")

add_library(kdoslook STATIC kdoscommand.cpp kdedclient.cpp accentmodel.cpp lookcontroller.cpp)
set_target_properties(kdoslook PROPERTIES POSITION_INDEPENDENT_CODE ON)

kcmutils_add_qml_kcm(kcm_kdos_look SOURCES lookkcm.cpp)
target_link_libraries(kcm_kdos_look PRIVATE kdoslook KF6::CoreAddons KF6::KCMUtilsQuick Qt6::Qml)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

The tests use `ecm_add_test` with `Qt6::Test`, and a test that drives a command points it at a
fake under `tests/` through a compile definition rather than at the real program.

## Names

Every name a piece introduces carries KDOS's prefix, so nothing it registers can collide with
KDE's or with an application's:

| Name | Form | Examples |
|---|---|---|
| C++ namespace | `Kdos` | `Kdos::KdosCommand`, `Kdos::LibraryShelf` |
| D-Bus service and interface | `io.kdos.*` | `io.kdos.Runner`, `io.kdos.Kded1`, `io.kdos.Slots1`, `io.kdos.Power1` |
| polkit action | `io.kdos.*` | `io.kdos.firewall.set`, `io.kdos.slots.bless`, `io.kdos.slots.install-route`, `io.kdos.ports.install`, `io.kdos.ports.build`, `io.kdos.installer.run` |
| System Settings module | `kcm_kdos_*` | `kcm_kdos_look`, `kcm_kdos_system` |
| Plasma package and plugin id | `org.kdos.*` | `org.kdos.privacy`, `org.kdos.meters`, `org.kdos.desk.tiling` |
| KWin effect and script id | `kdos_*` | `kdos_unfold`, `kdos_keys`, `kdos_runraise` |
| Translation domain | `kdos-<piece>` | `kdos-kcm-look`, `kdos-plasmoid-privacy`; `kcm-kdos-system` and `kcm-kdos-boxes` use their module ids, `kcm_kdos_system` and `kcm_kdos_boxes` |
| Exported C symbol | its library's prefix | see [The C libraries](c-libraries.md#the-set) |

`testing/preflight.sh` reads every KDOS `metadata.json` and plugin JSON for what Plasma and KCMUtils
refuse to load without (a name, a package id, a widget's API version), and refuses a KDOS widget,
global theme, desk layout or wallpaper whose id is outside `org.kdos.*`.

## The toolkit rule

Only code in `src/desktop` may link Qt 6 and KDE Frameworks 6, and no KDOS code links anything
else from the toolkit list: no GTK, no libadwaita or WebKitGTK, no Qt 5 or KF5, no wxWidgets,
FLTK or Tk, and no Xlib. Inside `src/desktop`, a program that is set-id, installed under `sbin`
or started by `rcS` is held to the full ban as well: code that runs as root or before anyone has
logged in links no toolkit. Every other area of `src/` keeps the full ban, so the root daemons,
`kpkg`, `kinstall` and `kdos-appbox` stay small and start before the session exists.

`testing/preflight.sh` enforces this on the built tree: it reads the `NEEDED` entries of every ELF
file a `src/` port installs, and of each library those name one level down, and names each file
that breaks its area's rule. It proves the guard first on `testing/fixtures/toolkit`, where a Qt
link in `src/system`, `src/daemons` and `src/art`, a GTK, Qt 5 or Xlib link in `src/desktop`, and a
Qt link from a set-id, `sbin` or `rcS`-started program must each be refused, and a module linking
Qt 6 and KF6 must pass. Why the desktop is Plasma and the rest of KDOS links no toolkit is in
[Principles](../01-philosophy/principles.md).

## Kinds of piece

### A System Settings module

A KCM is a QML page in a plugin. `kcmutils_add_qml_kcm()` builds the plugin and compiles the page
from `ui/main.qml` into it; the plugin's JSON beside the sources carries its name, icon and
description, the category it sits under (`X-KDE-System-Settings-Parent-Category`), its search
keywords (`X-KDE-Keywords`) and its help link (`X-DocPath`). The page is a `KCM.SimpleKCM`, or a
`KCM.ScrollViewKCM` for a page that is one long list, holding a `Kirigami.FormLayout`, and talks to
a C++ controller the plugin exposes, which is where the logic lives and what the tests drive.

`X-DocPath` names the module's section of this book under `help:/`:
`kdos/04-programs/kdos-settings.html#<anchor>`. System Settings' Help button opens `help:/` followed
by the key, and KIO's `help:` worker finds the page through `/usr/share/doc/HTML/en/kdos`, the link
`kdos-doc` installs to the book (see [Help](../04-programs/kdos-settings.md#help)).
`testing/anchorcheck.py` fails when the anchor names no heading of that page, so a heading renamed
without the JSON changing is caught.

### A Plasma widget

A plasmoid is built with `plasma_add_applet(org.kdos.<name> QML_SOURCES ... CPP_SOURCES ...)` and
carries a `metadata.json` with `X-Plasma-API-Minimum-Version` 6.0, which plasmashell needs before
it will load the widget at all. A widget that belongs in the system tray names its
`X-Plasma-NotificationAreaCategory`. A QML module several pieces import (the checklist's
`org.kdos.checklist`, which the Welcome pages also show) is built and installed as a module of its
own.

### A KRunner runner

`kdos-runner` is KRunner's D-Bus runner API answered in C: a `krunner/dbusplugins/*.desktop` file
with `X-Plasma-API=DBus2` names the service and object path, and a D-Bus service file starts the
program on the first query. It links `basu`, SQLite, `libkbase`, `libkxdg` and `libkcolor` and no
Qt, so it is not rebuilt for every Plasma release, and a crash costs KRunner one runner's rows rather than
its process. `Run` executes only the argument vector its own `Match` built for that row; an id it
did not hand out does nothing.

### KWin effects and scripts

KWin loads three kinds of package from `share/kwin`: a JavaScript effect (`KWin/Effect` with
`X-Plasma-API` `javascript`), a QML effect (`declarativescript`) and a KWin script (`KWin/Script`).
Each is installed with ECM's `kpackage_install_package` and keeps working across KWin releases,
because it reaches KWin through its scripting API rather than its C++ classes. A compiled effect
does not: KWin embeds its version in the plugin's interface id and refuses one built for another
release, so KDOS's one compiled effect is carried as a patch to the `kwin` port. A shortcut a
script or effect registers is checked against every other binding the session makes (see
[Shortcuts](#shortcuts)).

### A kded module

`kded-kdos` is a kded6 plugin: it loads with the session, watches configuration with
`KConfigWatcher` and devices with Solid, raises notifications through the events in its
`.notifyrc`, and serves `io.kdos.Kded1` at `/modules/kdos` for the pieces that ask it something.

### The Discover backend

`kdos-discover` links `libdiscover` and the notifier plugin API, both of which are internal to
Discover and change between Plasma releases, so the backend is rebuilt and its tests run on every
Plasma bump. It shows packs, boxes and ports as Discover resources, and its notifier puts the A/B
update into Plasma's update notification. The behaviour is in
[Discover](../04-programs/discover.md).

### Plasma packages and Welcome pages

`kdos-plasma-look` writes its packages at build time: `genlook.py` and `kdos-theme` expand the
palette into the eight global themes, the colour schemes, the Plasma style, the desk layouts and
KSplash, so no accent is ever spelled by hand. `kdos-welcome` installs Welcome Center's extra pages,
which a patch to `plasma-welcome` shows straight after the welcome page and before Plasma's own. A
piece that ships a package Plasma installs is checked by `kpackagetool6` in the test run.

### Session programs in C

A piece with no window of its own can be a C program on the session bus, as `kdos-runner` and
`kdos-notice` are. It links `basu` for D-Bus and the `libk*` libraries it needs, keeps the
[C libraries](c-libraries.md)' rules (no shell, no `system()`), and is tested by
`testing/selftest.sh` on a bare host, on a fake session bus where it needs one.

## Conventions

### Colour

A colour comes from the theme, never from a literal. In QML that is `Kirigami.Theme` (its
`textColor`, `highlightColor`, `negativeTextColor` and the rest, under the colour set the item
asks for); in C++ it is `KColorScheme`. The KDOS accents reach Plasma only as the colour schemes
generated from `libkcolor`'s palette, so a role follows the accent, the light and dark switch and
Phosphor mode, while a literal follows none of them and is unreadable on at least one of the eight
schemes. A swatch that shows another scheme reads that scheme's roles through `KColorScheme` too,
as `kcm-kdos-look`'s accent picker does.

`highlightColor` is the selection, not the accent: on a dark accent it is the accent darkened until
the view's text reads on it. A mark drawn to show the accent itself (a rule, an outline, a
sparkline, a swatch's stripe) reads `focusColor`, or `KColorScheme::FocusColor` in C++, which is
the accent held to 3:1 in every set but the Selection. Text in the accent (a heading, a readout)
reads `linkColor` or `activeTextColor`, `KColorScheme::LinkText` or `KColorScheme::ActiveText` in
C++, which hold it to the 4.5:1 text floor; `focusColor` on text fails that floor on any accent
that clears only 3:1. See
[the role mapping](../03-architecture/design-language.md#the-role-mapping).

`testing/preflight.sh` reads every `.qml`, `.js`, `.cpp` and `.h` under `src/desktop` (and the
QML and JavaScript under `src/art`) with comments removed, and fails a quoted `#rgb`, `#rrggbb` or
`#aarrggbb`, a `Qt.rgba`, `Qt.hsla` or `Qt.hsva` built from numbers, a `QColor` built from numbers
or a string, a `Qt::GlobalColor` other than `transparent`, and a named colour assigned to a colour
property. Tests are not read: their colours are the values they check a role mapping against. A
literal that is no scheme's to give is let through by an entry in the check's allow list naming
its file and its exact text, with the reason beside it; a whole file is never let through.

### Strings

Every string a person reads goes through KI18n: `i18n()`, `i18nc()` with a context, or `i18ndc()`
with the piece's domain in QML, and `KLocalizedString` in C++. The image ships English only (see
[Known gaps](../06-reference/known-gaps.md#every-program-speaks-english)), and the strings stay
translatable all the same. A context says what the string is (`@title:group`, `@label`,
`@action:button`), which is also what makes a short word unambiguous to the next reader.

### Layout and messages

A piece follows the KDE Human Interface Guidelines. A settings page is a `Kirigami.FormLayout`, its
groups `Kirigami.Separator` items with `Kirigami.FormData.isSection`, and its labels on
`Kirigami.FormData.label`. An error, a warning or a step the person must take is a
`Kirigami.InlineMessage` at the top of the page, closable where it can be dismissed. Sizes and
spacing come from `Kirigami.Units`, never from pixel counts, so a page scales with the font and
the output.

### Accessibility

Every control has an accessible name. A stock Kirigami or Qt Quick Controls control with a text of
its own has one already; anything else (a swatch, a lamp, an icon button, a custom delegate) sets
`Accessible.role` and `Accessible.name` to the words a screen reader should say. The AT-SPI gate
(see [Testing a piece](#testing-a-piece)) reads each KDOS module, Welcome page and installer page
as a screen reader would and fails a visible control without a name.

### Starting a program

A piece starts a program with `QProcess`, a program path and an argument list: `setProgram()`,
`setArguments()`, `start()`. Never `sh -c` and never a single command string: a box name, a file
name or a package name that reaches a shell is an injection point, the same rule the C libraries
keep with their argument-vector builder. Find the program with `QStandardPaths::findExecutable()`,
and for the `kdos` command fall back to `/usr/local/bin/kdos`, which a session started without a
login shell may not have on `PATH`. Give a command a timeout that kills it, and read its standard
error so a failure can be shown with its reason. A command that must finish once started (an
accent switch) is left to finish when its page closes rather than killed half done.

### Reaching the root daemons

A piece never runs as root. It reaches what needs root one of two ways:

- **A daemon on the system bus.** `kdos-powerd` serves `io.kdos.Power1` (the firewall, the accent,
  the splash) and `kdos-sysupdated` serves `io.kdos.Slots1` and `org.freedesktop.sysupdate1`.
  Each asks polkit about the caller for each method that changes something, so the piece calls the
  method and the polkit agent prompts when an administrator must approve. A module that shows a control as available or
  not asks polkit first without a prompt, as the firewall module does.
- **pkexec on one program.** Where a whole operation runs as root (installing a port, building
  one, installing the system), the piece runs `pkexec` on one helper whose path a polkit policy
  names in `org.freedesktop.policykit.exec.path`: `kdos-ports-helper` under
  `io.kdos.ports.install`, `kdos-ports-builder` under `io.kdos.ports.build`, and `kinstall` under
  `io.kdos.installer.run`. pkexec takes the action whose path matches the program it is asked to
  run, so the policy and the program's installed path must agree, and two operations that must not
  share a grant are two programs, one per action: the builder is a separate binary so that a
  password kept from a prebuilt install never covers a build.

The daemons, their interfaces and their grants are in [The daemons](../04-programs/daemons.md) and
[the security model](../03-architecture/security-model.md).

### Shortcuts

A global shortcut is declared where kglobalaccel or KWin reads it: a desktop entry's
`X-KDE-Shortcuts`, a `kglobalaccel` service file, a KWin script's `registerShortcut()`, or a QML
effect's `ShortcutHandler` with literal `name:` and `sequence:` strings. `testing/chordcheck.py`
builds the session's whole set of bindings from KDE's defaults, the input method's hotkeys, the
shipped `kglobalshortcutsrc` files and every KDOS script and effect, and fails a chord two owners
want, because the one that asks first gets it and the other is told nothing.

### Data packs in the library

A data pack (offline encyclopedias, maps, models, documentation sets, courses) makes its items
appear in the offline library through a desktop entry it installs: `Categories=X-KDOS-Library;`,
`X-KDOS-Library-Kind=` one of `Books`, `Maps`, `Models`, `Docsets` or `Courses`,
`X-KDOS-Library-Path=` the item's absolute path, and `X-KDOS-Pack=` the pack's id. `kio-library`
reads these keys, and the entry's `Name=` and `Icon=` win over what its own scan of the known
directories finds; the same entry is the item's row in the menu's Library category.

## Testing a piece

KDOS's Qt code is tested where Qt, KDE Frameworks and Plasma's development files are installed,
which is the development image (`testing/devdeps-image.sh`), not a bare host:

```sh
testing/devdeps-image.sh plasmatest                  # every piece, in the development image
testing/devdeps-image.sh plasmatest kcm-kdos-look    # one piece and what generates it
testing/devdeps-image.sh plasmatest --selftest       # the harness on its own fixtures
```

Inside the image, `make plasmatest` (with `PLASMATEST_ARGS`) runs the same script.
`testing/plasmatest.sh` refuses on a bare host, naming what is missing, rather than skip and read
as a pass. It runs four stages, each a failure of its own:

1. **Build and QtTest.** Each piece is built by its own `build.sh` with `BUILD_TESTING=ON`, and its
   tests run under `ctest --output-on-failure`, headless (`QT_QPA_PLATFORM=offscreen`), with a
   fresh home and each test on a session bus of its own, so tests that serve fakes of the same
   D-Bus name do not take it from each other.
2. **Plugins.** Every plugin a piece installs is loaded with `QPluginLoader`: a missing symbol, a
   library the image lacks or metadata with no name is a plugin Plasma drops without a word.
3. **Packages.** Every Plasma package (a look-and-feel, a desk, a Plasma style, a plasmoid, a KWin
   script or effect) is installed with `kpackagetool6` into a scratch root under its package
   structure, and the login screen's theme is checked for what the greeter loads.
4. **qmllint.** Every QML file is linted against the installed QML modules and the ones just built.
   What a file reads from a context property (a KCM's `kcm`) or a type registered at run time is
   reported and does not fail; anything else qmllint finds does.

The accessibility gate reads each surface's AT-SPI tree as a screen reader does and compares a
text dump of it with that surface's golden under `testing/atspi/goldens`.
`testing/atspi/headless.sh` dumps a piece built in the development image in about a minute, on
weston's headless backend rather than KWin; `make atspi` (`testing/atspi/golden.sh`) boots the
shipped image and dumps every surface named in `testing/atspi/surfaces` inside the Plasma session.
Only `make atspi ATSPI_ARGS=--update` writes a golden, because a golden describes the shipped
image. Either fails a visible control without a name. `make check` runs the gate's
own fixtures (`atspi-selftest`) and every preflight check above, but not the Qt suite, which needs
the development image.

The C pieces (`kdos-runner`, `kdos-notice`) and the C helpers are `testing/selftest.sh`'s, on the
host. [Testing](testing.md) describes every harness.

## Trying a change on the desktop

Build the one port in its phase:

```sh
make build BUILD_ARGS="--phases 50_desktop --rebuild kcm-kdos-look"
```

To see it on a running desktop without packaging an ISO, `testing/quick.sh` builds the port into
`build/fs`, tars exactly the files the package owns, boots the last ISO built and untars them over
its live session, then restarts what reads them:

```sh
testing/quick.sh plasmoid-kdos-privacy -- --sleep 3 --shot build/shots/x.png
KDOS_QUICK_KEEP=1 testing/quick.sh kcm-kdos-look -- ...   # a module is started fresh; restart nothing
```

KWin is restarted for KWin and its plugins, plasmashell for a widget or a Plasma package, and an
effect, a KWin script or a kded module is reloaded in place; `KDOS_QUICK_RESTART` names exactly
what to restart instead. A KCM, Discover or the installer runs the new code the next time it is
opened. Nothing that owns a D-Bus name on the session bus can be tried this way, because a
restarted owner races the old one for the name, and neither can the login screen's theme, a new
port or anything under `fs/`. What this shows is not evidence about the shipped image: that is the
ISO `make build` writes. See [Testing](testing.md#the-qemu-rig).

## Pieces tied to a Plasma release

Most pieces use stable API: KCMUtils, Kirigami, KRunner's D-Bus interface, kded and KWin's
scripting API keep working from one Plasma release to the next. Four do not, and each is rebuilt
and its tests run on every Plasma bump:

| Piece | Tied to |
|---|---|
| `kdos-discover` | `libdiscover` and Discover's notifier plugin API, both internal to Discover |
| `ksystemstats-kdos` | `libksysguard`'s sensor plugin and face API, which are not stable |
| The `kwin` patches, the CRT effect among them | KWin's internal C++ API and its plugin version |
| `kdos-welcome` | `plasma-welcome`'s extra pages, through the patch carried on it |

The bump procedure is in [Writing ports](writing-ports.md#kde-frameworks-plasma-and-gear).

## The checklist

- [ ] The piece is a port at `src/desktop/<name>/`, named in the `src-desktop` section of
      `50_desktop`'s list, with a `kfs.md` beside its recipe.
- [ ] Every name it introduces carries KDOS's prefix: `Kdos`, `io.kdos.*`, `kcm_kdos_*`,
      `org.kdos.*`, `kdos_*`, `kdos-<piece>`.
- [ ] It links Qt 6 and KF6 and nothing else from the toolkit list, and nothing it installs is
      set-id, under `sbin` or started by `rcS`.
- [ ] Every colour is a `Kirigami.Theme` or `KColorScheme` role.
- [ ] Every string goes through KI18n with a context.
- [ ] Every control has an accessible name.
- [ ] Every program it starts is a `QProcess` with an argument list, and everything that needs root
      goes through a daemon's bus interface or `pkexec` on one helper a polkit policy names.
- [ ] Its logic sits in a library its QtTest suite links, and
      `testing/devdeps-image.sh plasmatest <port>` passes.
- [ ] A settings module's `X-DocPath` names a heading that exists in the book.
- [ ] Its behaviour is written in its Part IV chapter, and `python3 testing/booknav.py`,
      `bash testing/docscheck.sh` and `python3 testing/anchorcheck.py` pass.

## See also

- [The design language](../03-architecture/design-language.md) — the accents, the colour roles,
  fonts and motion every piece follows
- [KWin](../04-programs/kwin.md) — KWin's configuration, patches, effects and scripts
- [The Plasma shell](../04-programs/plasma-shell.md) — the runner, the widgets and the Welcome pages
- [System Settings](../04-programs/kdos-settings.md) — the KDOS modules and the System Monitor
  sensors
- [Discover](../04-programs/discover.md) — the backend, the notifier and the update daemon
- [The daemons](../04-programs/daemons.md) — the root daemons a piece reaches and their grants
- [Writing ports](writing-ports.md) — the recipe format and the KDE build shape
- [The C libraries](c-libraries.md) — what a C piece or helper compiles in
- [Testing](testing.md) — preflight, the Qt suite, the AT-SPI gate and the rig
- [Principles](../01-philosophy/principles.md) — why the desktop is Plasma and the rest links no
  toolkit

<!-- book-nav -->
---

*Part V — Building and developing, chapter 36.* Previous: [35. The C libraries](c-libraries.md) · [Contents](../README.md) · Next: [37. Testing](testing.md)
