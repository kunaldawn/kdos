# Testing

This chapter describes how to find out whether a change to KDOS works without waiting for a full
build, which takes hours. It is written for anyone changing the tree: a recipe, a library, a daemon,
a System Settings page or a widget, a shipped configuration file or a page of this book. Each
harness under `testing/` is described in turn with what it proves, what it cannot prove, how long
it takes and how to run it. Read [Developing](developing.md) first for the build and its iteration
loops, which these harnesses fit into.

KDOS ships no test binaries. Its checks fall into four families:

- **Offline checks over the tree**, which need no build and no emulator. `preflight.sh` checks the
  wiring of the tree in two to three minutes, and with it the defaults KDOS gives KDE's programs,
  its shortcuts and its default applications. `selftest.sh` compiles the C libraries, the daemons
  and every C program that uses them and runs their assertions in a few minutes; part of it is the
  *fixtures*, recorded system state and stand-in peers that a program can be pointed at instead of
  the live machine. `docscheck.sh`, `anchorcheck.py` and `booknav.py` check this book and *KDOS
  from Scratch*, and `kfscheck.py` checks the second book against the recipes it is generated from.
- **The Qt suite**, `plasmatest.sh`, which builds KDOS's Qt code (its System Settings pages,
  widgets, KWin plugins, Discover backend and installer) with its QtTest suites, loads its plugins,
  installs its Plasma packages and lints its QML. No bare host has what it needs, so it runs in the
  development image.
- **A source check**, `make fetch-check`, which confirms every upstream source a recipe names is on
  disk and matches its hash.
- **Boot-time harnesses** built on the *rig*, a virtual machine that boots the real image, drives
  it and photographs it: `vnc-shot.py` is the rig's driver; `plasma-probe.sh` checks that the
  Plasma session is whole, `session-cost.sh` times it and weighs it, the AT-SPI harness reads every
  KDOS surface's accessibility tree against its *golden*, and `quick.sh`, `packlane.sh`,
  `install-to-disk.sh` and `bios-boot.sh` are built on the rig or beside it.

A *surface* here is one window or page KDOS draws in the session: a System Settings page, a Welcome
Center page, the installer. The terms *surface*, *golden*, *fixture* and *rig* are also defined in
the [glossary](../06-reference/glossary.md). Start with [What each tool proves](#what-each-tool-proves)
and [Which one to run](#which-one-to-run), then read the section for the harness you need.

## What each tool proves

| Tool | Proves | Cannot prove | Takes |
|---|---|---|---|
| `testing/preflight.sh` | The wiring: every reference resolves, every option exists, every script parses, every KDE family is one release, KDOS's code links only the toolkit its area allows, and the shipped shortcuts, window rules, desks and default applications hold | That anything works | Two to three minutes |
| `testing/selftest.sh` | The libraries' invariants, that every C consumer still compiles, that the build orchestrator and package manager behave, and that the daemons and the `kdos` command answer their peers as those peers expect | That the system boots | A few minutes on a bare host |
| `testing/plasmatest.sh` | That every KDOS Qt port builds and passes its QtTest suites, that its plugins load, that its Plasma packages install, and that its QML lints | That a page looks right or reads well in the session | Minutes, in the development image |
| Fixtures | That a reading or a decision is correct against recorded state, or that a program speaks a protocol the way its real peer expects | That the reading is correct on a live machine | Part of the self-test and preflight |
| `testing/defaultscheck.py`, `testing/xdgcheck.sh` | That every default KDOS gives a KDE program reads back as intended through the configuration cascade, and that those defaults stay out of `/etc/xdg` | That the program honours the key | Seconds |
| `testing/vnc-shot.py` | That a real session does a thing, photographed | Anything the renderer in use cannot show | Minutes per boot |
| `testing/plasma-probe.sh` | That the booted session is whole: its processes, its bus names, logind's view of it, its desk and its windows | That anything in it is usable | One boot |
| `testing/flows/` | That the login paths work end to end: autologin, the greeter, logout, the lock screen, suspend and resume, VT switching, and that no key typed into Plasma reaches a tty; flow 05 also that the screen is lit after resume | How the greeter and the lock screen look | One boot per flow |
| `testing/session-cost.sh` | How long the boot takes to reach the panel, and what the session costs in memory at idle | Anything about hardware other than the rig's | One boot |
| AT-SPI goldens | That a KDOS surface's accessibility tree has not changed, and that every visible control in it has a name a screen reader can say | That it is laid out well | One boot for the gate; a minute per surface headless |
| `testing/quick.sh` | That a changed component behaves in a booted session, without repacking the ISO | Anything about the shipped image | A few minutes |
| `testing/docscheck.sh` | That both books' links resolve, that they avoid the common historical phrasings, that each page keeps its book's contract, and that the port counts and catalogue rows match the tree | Anything a reader has to judge | A few seconds |
| `testing/anchorcheck.py` | That every `#fragment` link in both books, `README.md` and `CLAUDE.md` lands on a heading, and that every help address a KDOS settings page names is a section that exists | That the heading says what the link promises | Under a second |
| `testing/booknav.py` | That every chapter footer, and the contents of *KDOS from Scratch*, follow the reading order | Whether the order is a good one | A few seconds |
| `testing/kfscheck.py` | That *KDOS from Scratch*'s notes, includes, kernel option table and recipe stamp match the tree, that every port renders, and that a new or changed patch carries a `Subject:` line | That a note is still true | A few seconds |
| `make fetch-check` | That every source a recipe names is on disk and matches its hash | That the sources build | About a minute, offline |
| `testing/packlane.sh` | The pack lane (installing boxed applications from packs; see [Packs and boxes](../03-architecture/packs-and-boxes.md#two-lanes-one-box)) end to end on a booted machine | Anything without application packs to install | Minutes |
| `testing/install-to-disk.sh` | That the installer installs, and what it wrote | That the installed system boots, which is the second half of the run | Minutes, and host space for the whole payload |
| `testing/bios-boot.sh` | That the image boots on a machine with no UEFI firmware at all | Anything about the UEFI path | About two minutes |

None of them proves the build works. A package manager and a build orchestrator are only fully
tested by building the distribution with them.

## Which one to run

| You changed | Run |
|---|---|
| A phase list, a recipe, a `depends` line, anything under `script/` or `fs/` | `testing/preflight.sh` |
| A library under `src/libs/`, a daemon, or a C program of KDOS's own | `testing/selftest.sh` |
| A parser | `CC="cc -fsanitize=address,undefined -g" testing/selftest.sh` |
| A Qt port under `src/desktop`, a QML file, a Plasma package or the SDDM theme | `testing/devdeps-image.sh plasmatest <port>` |
| A default KDOS gives a KDE program, under `fs/usr/share/kdos/xdg` or `fs/etc/xdg` | `make defaultscheck xdgcheck`, then the row in `testing/fixtures/defaultscheck/defaults.tsv` if the change is intended |
| A shortcut, a window rule or the default-applications list | `testing/preflight.sh`, which runs `chordcheck.py` and `mimecheck.py` |
| A KDOS surface's controls or labels | `testing/atspi/headless.sh <surface>` in the development image, then `make atspi` on a real ISO, and `ATSPI_ARGS=--update` once the change is intended |
| A recipe's `sha256 =` line or its sources | `make fetch-check` |
| Something only a running session shows | `testing/quick.sh`, then the rig on a real ISO |
| The session itself: login, startup, a session daemon | `make session-probe` on a real ISO |
| A page of either book | `bash testing/docscheck.sh`, `python3 testing/anchorcheck.py` and `python3 testing/booknav.py` |
| A chapter added, removed or reordered | `python3 testing/booknav.py --write`, after editing `docs/kdos/README.md`'s contents for this book |
| A recipe, a patch, a `kfs.md`, a phase list, `kdos.config` or a script a page of *KDOS from Scratch* includes | `python3 testing/kfscheck.py`, then `make book-stamp` when it reports the stamp stale and `kfscheck.py --repin` when it reports an include range moved |
| Nothing, but a build finished and a phase is about to be built with `--port-jobs` | `python3 testing/depdrift.py` in the development image |
| A recipe's flags, `depends` or installed files, once its port is built | `make debuginfo`, `make linkcheck` and `make entrycheck`, from a container or as root; `70_image` runs the last two and stops on them |

Each offline check has a make target of its own name: `make preflight`, `make selftest`,
`make docscheck`, `make anchorcheck`, `make booknav`, `make kfscheck`, `make phaseclosure`,
`make defaultscheck`, `make xdgcheck`, `make lnfcheck`, `make aboutcheck` and `make atspi-selftest`. `make selftest-asan` runs the
self-test under the sanitizers, with the compiler in `SELFTEST_CC`. `make check` runs all of them
but the sanitized self-test, in that order, in one command. `make plasmatest` is not part of it,
because it needs the development image. The full list of targets is under
[Make targets](developing.md#make-targets).

## preflight.sh

Preflight checks the tree's wiring without building anything. It runs 74 checks:

```sh
testing/preflight.sh
```

It first compiles its own copy of `kpkg` (the package manager) from `src/`, into a temporary
directory, and invokes it under the five names it is installed as. It then runs the checks, each
headed by a `==>` line. It exits 0 when every check passes and 1 when any fails, prints each
failure with the file to look at, and ends with either `preflight clean` or the number of problems
found.

Several checks read the built tree under `build/fs`. Without one, each says what it left out
rather than passing quietly: orphaned packages, the links of KDOS's own programs, the file modes
and the polkit and udev rule ownership, the built `kinstall`, the console font's glyphs, desktop
entries, the entries the application menu may list and the `kdos-*` manual pages print
`skipped — no build tree`, and the programs a recipe's embedded scripts name, the programs
`fs/etc/inittab` names, the `mimeapps` rows and the build tree's root run a reduced check from `fs/`
and the recipes and say so. A partial `build/fs` counts as a build tree, and a check that finds its
directory there reports what is missing from it.

| Group | Checks |
|---|---|
| Layout | Every recipe under `ports/core` sits exactly at `ports/core/<shelf>/<name>/kpkgbuild`, and `ports/core` holds nothing but shelves; `ports/shelves` lists every shelf once, each with a volume that is a positive integer and a description, an id of lowercase letters, digits and `-`, not `libs` or `core` and not a port's name, and each a non-empty directory; `src/` holds only its six areas and every port of KDOS's own sits at `src/<area>/<name>/`; the orphan sweep searches every `src/` area that holds a recipe; every bare name is one port across the shelves and the `src/` areas; every `name =` is its directory's name; every `group =` family sits on one shelf; every file of a `packages.d/` is `00-order.txt`, a listed shelf's `<shelf>.txt` or a `src-<area>.txt`, and names only ports filed there, and no phase has both `packages.txt` and `packages.d/` |
| Packages | Each package phase from `30_foundation` on installs exactly the ports its list names, and each port of `41_system` to `44_apps` is in the phase the phase table gives its closure, computed by `testing/phaseclosure.py` (see [Every phase installs exactly its list](build-system.md#every-phase-installs-exactly-its-list)); every package named in a phase list has a port; ports built from one tarball agree on its version and hash (`linux` and `perf`, `python3` and `python3-tkinter`, `gettext` and `libintl`, `glib` and `glib-introspection`, `llvm` with each of its subprojects, and the other pairs the check lists); each KDE family, KDE Frameworks, Plasma and KDE Gear, is at one release (see below); every Gear application nothing else depends on is named by exactly one `kde-gear-*` meta port; every list resolves to a dependency order with clean output and valid tokens; every `depends` entry names a port that exists, and names no more than the 128 `kpkg` reads; every port of KDOS's own is built by something; the build tree carries no package whose port is gone or listed in `ports/unbuilt` |
| Recipes | Every port has a build script and it parses; every recipe parses as metadata; every one declares a name, version and release; every recipe and build script carries the KDOS banner; every source a port declares is named by a checksum and is non-empty where it is on disk (one not yet fetched is reported as a `make fetch` note, not a failure); every recipe carries a `description`, a `homepage` and a `license` that is an SPDX expression, its first source spells the version as `$version` (`docbook-xml` excepted), and its `description` is exactly the one its row in the [ports catalogue](../06-reference/ports-catalogue.md) gives, every port having one row there (all read by `testing/recipemeta.py`); the switches that turn off a feature whose library is a port are counted by `testing/optdeps.py`, a report that never fails; every `.patch` or `.diff` under `ports/` or `src/` that is new or changed (on no remote yet) has a `Subject:` line in its mail header, by `testing/kfscheck.py --only patches`, with `KDOS_CHECK_BASE=<ref>` measuring against a ref instead (see [kfscheck.py](#kfscheckpy)) |
| Build options | Every meson option a recipe passes is one that port defines, checked against the tarball's own option file, with the two closed-value option types validated; every recipe that runs `meson setup` names its buildtype; every `go build` and `go install` passes `-s` and `-w` in its `-ldflags`, read with its backslash continuations joined; every `pip install` into a package root (`--root`) passes `--ignore-installed`, or pip removes the build root's own copy of what it installs; `script/bin/ninja`, run against a stand-in ninja, adds `-j$KDOS_JOBS` only to a call that gives no job count, passes `-t`, `--version` and a given `-j` through unchanged, and runs `/usr/bin/ninja` by absolute path; `script/bin/meson` adds `--wrap-mode=nodownload` to an explicit or implicit `setup` that names no wrap mode and hands `compile`, `install`, `configure`, `subprojects` and `--version` on unchanged; every recipe that runs cargo builds against a shared C library |
| Sources | Every source file in a port of KDOS's own is compiled by its recipe, unless that recipe globs its own `$PORT_SRC` directory (a glob of the shared `libk*` trees does not exempt it) or runs CMake on it, in which case the files its `CMakeLists.txt` names count as compiled; a first source whose members are `./`-prefixed is accounted for; a flat first source is unpacked by its own recipe |
| The sources archive | No archive a recipe hashes is tracked by git; `.gitignore` ignores every archive suffix under `ports/core` and the source cache, and none of the archive fixtures under `testing/fixtures`; `ports/srclib.sh`, `ports/fetch`, `ports/publish` and `script/hooks/pre-push` exist, are executable where they must be, and pass `bash -n`; `ports/sources.idx` holds every format-2 rule: `# kdos-sources-index 2` on line 1, each line `<sha256> <tag> <asset> <port>/<file>` with an optional `parts=<N>:…` of 2 to 99 hashes, each tag `sources-<N>` with `N` a positive integer, or a `src-<name>` of the older layout, which passes and is counted in a `WARNING` note saying `make publish-rehome` moves those lines, one line per hash, no part hash equal to a line's hash, no two asset names in one tag equal ignoring case (a split file's parts and its bare name counted), and hash order. An unset `core.hooksPath` is printed as a note, not a failure |
| The session | No systemd unit ships: a `.service` is a D-Bus service file under `usr/share/dbus-1` or a libaccounts service description under `usr/share/accounts/services` and nowhere else, no `.socket` ships, and a `.timer` lives only in the tables KDOS hands to `snooze` (`etc/kdos/timers.d`, `etc/kdos/user-services/timers.d` and skel's `.config/kdos/timers.d`); `startkderc` tells `startplasma-wayland` there is no systemd; SDDM's greeter runs on Wayland and nothing configures an X server, with `fs/etc/X11` absent; every program the About page's `kcm-about-distrorc` runs prints a label and a value (see `aboutcheck.sh` below); no `profile.d` script exports `XCURSOR_THEME`, which KWin would prefer over the Cursors page; "Get New…" is off for every application through `[KDE Action Restrictions] ghns=false`; every program `fs/etc/inittab` names is on the image; every filesystem the installer offers, the initramfs can mount; the ISO step builds every boot path (BIOS and UEFI, disc and written stick); the built `kinstall` is the installer in this tree |
| Plasma packages | Every KDOS plasmoid, KCM and look-and-feel carries a `metadata.json` that parses and has what Plasma keys on (an `org.kdos.*` id, `X-Plasma-API-Minimum-Version` 6 for a plasmoid); the shipped `kglobalshortcutsrc` and `kwinrulesrc` parse, every chord has one owner and every `[services]` group names a desktop entry some port installs (by `testing/chordcheck.py`), and `kwinrulesrc`'s count and rule list agree with its groups; every desk's `layout.js` parses (with `node`; skipped without it); every desktop id `/usr/share/kdos/xdg/kde-mimeapps.list` names is installed (by `testing/mimecheck.py`); every command a KRunner route in `menu.conf` names exists |
| Permissions and accounts | `/etc/shadow` on the built tree is mode 600 or 640 and the file-system step narrows it; the polkit and udev rule directories and files are owned by root; no udev rule sets `GROUP=`, `MODE=` or `OWNER=` on a device class that has no node, and every group a rule grants is one the desktop user is in; every account a shipped daemon drops to exists in `fs/etc/passwd`; the desktop user is in the groups its pages need (`lpadmin` for the Printers page) |
| Shell | All shipped and build shell is syntactically valid; every file a build script, a `phase.env` or a shared environment file sources exists, and a step sources its own phase's `phase.env`; a script a recipe ships inside a `KDOS_SH` heredoc parses too, and every program it names as the first word of a line is one the image carries; no build script names a command inside double quotes and runs it; every helper the Makefile runs is on disk |
| Consistency | The build tree's root carries nothing but a root filesystem; KDOS's code links only the toolkit its area allows (see below); KDOS's desktop code takes every colour from a role, never a literal; every daemon an init script starts is installed by a port; every script under `fs/etc/init.d` but `service_helper` is executable in the work tree and recorded as `100755` in git; the root filesystem carries no script whose interpreter is gone; nothing points at a removed file or invokes a retired tool; every `port:`, `path:`, `see:` and `cite:` a recorded reason names still resolves; no chroot step reads the ports tree through `/kdos/ports`; the compiler cache sets no `base_dir`, checks the compiler by a string rather than a modification time, has its `KDOS_CCACHE` switch both passed by the `Makefile` and named by `script/chroot/exec.sh`, and puts no masquerade directory on a `PATH`; the package store's `KDOS_PKG_STORE` and `KDOS_PKG_STORE_MAX` are passed by the `Makefile`, `exec.sh` names those two and `KPKG_STORE`, `KPKG_STORE_CHECK` and `KPKG_STORE_SALT`, and `make cleanbuild` keeps `build/pkgstore`; the catalogue's rows match the tree, and no `app` row carries a package that is a host port's name (bar the libraries and firmware the lint allows), a check that first proves itself on a clean fixture row and a `kcalc` row it must flag; the application catalogue is installed by a port; a frame a terminal tool draws that opens the synchronized-output bracket closes it on every path; in the terminal tools, a literal colour is set at the render boundary and nowhere else |
| Shipped data | `mc`'s shipped rows name programs that exist; every `kdos-*` handler in the shipped `mimeapps` lists is on the image; the generated `aerc` styleset is one `aerc` will load; every `kdos-*` command on the built tree has a manual page |
| Terminal chrome | Every glyph in `libktui`'s UTF-8 table is one the shipped console font can draw; no text draw under `src/` takes `KT_DIM` as its foreground, except a rule, shade or block glyph, a blank cell, or a call marked `/* not text */`; no text draw takes `KT_SURFACE` or `KT_BG` ink on a selection fill; every visible desktop entry's icon and command exist on the image (an icon that is a symlink in its theme counts), and no two visible entries share a `Name=`; no visible entry reads as a server, session picker, launch pad, tray helper or diagnostic unless `testing/visible-entries.allow` names it, and that file names only entries that are visible and read that way |

Preflight cannot prove the build works. It proves the build will not fail for a mechanical reason
such as a missing port, a misspelt option or a script that does not parse. Run it after touching a
phase list, a recipe, or anything under the build scripts.

### Checks that catch what a compiler cannot

Five of these checks catch a whole class of failure that never reaches a compiler.

The first is that every source file in a port of KDOS's own is compiled by its recipe. A file the
build script neither names nor matches passes every gate on a development machine, because the
self-test globs those directories, and then fails to *link* in the build. A whole page can be
exercised by the harness and be absent from the shipped binary. A port whose `build.sh` runs CMake
on its own directory compiles what its `CMakeLists.txt` names, so a file named there counts.

The second is that every meson option is one the port defines. meson fails at setup on an unknown
option, before a line is compiled, and there is no universal spelling. Without this check a
misspelt option costs an hour-long round trip through the build to find.

The third is that **each KDE family is one release**. KDE Frameworks, Plasma and KDE Gear are each
released and tested as one set, and a tree holding two releases of a family builds without a
complaint and then fails at run time in ways no recipe shows. A port belongs to a family by the
directory its tarball comes from on `download.kde.org` (`stable/frameworks`, `stable/plasma`,
`stable/release-service`), or by `group = plasma` or `group = gear` when its source is elsewhere,
and every port on the `kf6` shelf is a Framework. A Plasma or Gear release is the first three
version components and a Frameworks release the first two, so a respin belongs to the release it
fixes; Plasma's side releases, such as `plasma-wayland-protocols`, belong to no family. The check
proves itself on `testing/fixtures/releaseset` first: the set passes, and a copy with one port moved
to another release fails.

The fourth is the **toolkit guard**: KDOS's code links only the toolkit its area allows.
Applications may link any toolkit. Code under `src/desktop`, KDOS's Plasma code, may link Qt 6, KDE
Frameworks 6 and the Plasma libraries built on them, and nothing else from the toolkit list: no
GTK, no libadwaita, no WebKitGTK, no Qt 5, no wxWidgets, FLTK or Tk, no Xlib. Code under
`src/system`, `src/daemons`, `src/art` and `src/devtools` links no toolkit at all, and neither does
a set-id file, anything under `sbin`, or a program `rcS` starts, whatever its area: root,
early-boot and library code that pulls in a GUI stack makes the login depend on that stack. Every
ELF file a port under `src/` installs is read for its `NEEDED` entries, and so is each library they
name, one level down, because a daemon linking a Plasma library links Qt through it. `libxcb` is not
on the list and Xlib is refused only when named directly: Xwayland speaks XCB, and the Frameworks
the desktop links reach Xlib themselves. The guard reads ELF headers itself, with Python and no
`readelf`, and proves itself on `testing/fixtures/toolkit/cases`, built with `cc`, before it reads
`build/fs`; without a build tree only the proof runs.

The fifth is that every glyph the terminal tools draw is one `ter-kdos32n`, the console font,
carries. `libktui`, which draws `kdosbuild`, the installer's terminal wizard and the other terminal
tools, picks its UTF-8 glyph table whenever the terminal reports UTF-8, which the Linux console
does, and the console font holds 512 glyphs. An entry that font lacks renders on a text console as
a blank: the cell is written, the flush succeeds, and a piece of the tool's own frame is missing.
`▓`, the half blocks `▀ ▄` and the double tees `╠ ╣ ╦ ╩` all look reasonable in an editor and none
of them is in the font. The check reads the shipped font itself, because a hand-kept list of what it
carries is the thing that goes stale.

### Colour in KDOS's desktop code

The KDOS accents reach Plasma only as the `.colors` files generated from `libkcolor`'s palette, and
every Qt surface reads them back through a role: `Kirigami.Theme` in QML, `KColorScheme` in C++. A
literal in `src/desktop` is a colour no accent, no light or dark switch and no Phosphor mode can
reach, and on one of the eight schemes it is unreadable. Preflight reads each `.qml`, `.js`, `.cpp`
and `.h` under `src/desktop`, and each `.qml` and `.js` under `src/art`, with its comments removed,
for a quoted `#rgb`, `#rrggbb` or `#aarrggbb`, a `Qt.rgba`, `hsla` or `hsva` or a `QColor` built
from numbers, a string or a `Qt::GlobalColor` other than `transparent`, and a named colour assigned
to a colour property. A test under `tests/` or `autotests/` is not read: its colours are the known
values it checks a role mapping against. The allow list names each literal that is no scheme's to
give by its file and its exact text, with the reason beside it, so a second literal in the same
file still fails. The check proves itself on `testing/fixtures/colourlit` first.

### The desktop's defaults

KDOS's defaults for KDE's programs live in `/usr/share/kdos/xdg`, the last directory of
`XDG_CONFIG_DIRS`, so that `/etc/xdg` stays the machine owner's: a key set there outranks KDOS's,
and an image update never collides with an edited `/etc` file. Several scripts hold those defaults
to what they are meant to do. Three run in `make check`, with the proof of a fourth; the rest run from preflight or on their own.

| Script | Checks | Run by |
|---|---|---|
| `testing/defaultscheck.py` | Every row of `testing/fixtures/defaultscheck/defaults.tsv`, one key each, reads back as the table says, through the cascade `/etc/xdg:/usr/share/kdos/xdg` with the earlier directory winning a key, and with `/usr/share/kdos/xdg-vm` or `/usr/share/kdos/xdg-live` in front for a `vm` or `live` row. Its own KConfig reader handles nested groups, comments, escapes, `[$e]` expansion and `[$i]` immutability, so it needs nothing but Python; `--kreadconfig CMD` reads every row with the real `kreadconfig6` instead, which proves the reader agrees with KConfig; `--root build/fs` reads a built tree; `--selftest` breaks each kind of row and requires each to fail. Defaults a port of KDOS's own installs are read from that port's source | `make defaultscheck`, `make check` |
| `testing/xdgcheck.sh` | No file under `fs/etc/xdg` but the ones that must be there (`kdeglobals` with only `[KDE] LookAndFeelPackage`, `konsolerc`, `fcitx5/config`, `mimeapps.list`, `autostart/`, `menus/`); no file shipped under both `fs/etc/xdg` and `fs/usr/share/kdos/xdg` but `kdeglobals`; a login shell's `XDG_CONFIG_DIRS` is `/etc/xdg:/usr/share/kdos/xdg`; with `--root`, none of the files the installer writes per machine (`kxkbrc`, `kdosrc`, `kwinrc`) is in the image's `etc/xdg` | `make xdgcheck`, `make check` |
| `testing/chordcheck.py` | No chord KDOS ships is bound twice. It builds the session's bindings from KDE's compiled-in defaults (a table measured from the Plasma and Frameworks sources), the `X-KDE-Shortcuts` of desktop entries, the `registerShortcut()` calls of KWin scripts and the `ShortcutHandler` blocks of KWin's QML effects and scripts, the shipped `kglobalshortcutsrc` along the cascade, and fcitx5's compiled-in hotkeys and the shipped `fcitx5/config`. A chord with two owners fails when one of them is a KDOS file; one between two upstream defaults is reported. `--live` reads what `kglobalaccel` holds in a running session instead | Preflight |
| `testing/mimecheck.py` | Every desktop id in `/usr/share/kdos/xdg/kde-mimeapps.list` is installed: with `--root`, in the image; otherwise traced to `fs/`, to a port of ours, to a `build.sh` that writes it, or to the fetched tarball of the port it names (an id whose tarball is not fetched is reported as unverified, and fails only under `--strict`). The list itself must hold only the specification's three groups, a MIME type or scheme as every key, once per group | Preflight |
| `testing/monofont.py` | The KDOS monospace face, JetBrains Mono, is first for `monospace` in the shipped `45-kdos-mono.conf`, and with the fallbacks that file names covers box drawing, block elements, braille, Latin Extended-A and -B, Greek and Cyrillic; its cell, measured on `M`, is the advance of every covered glyph; its zero is marked and its `1`, `l` and `I` differ. It records the cell and line height at 10 pt and scales 1, 1.25 and 2. Needs fontTools; `--selftest` builds a face that fails each gate | The self-test's last block (`--selftest`); by hand with `--root build/fs` |
| `testing/aboutcheck.sh` | Every program the About page runs prints exactly two non-empty lines, a label and a value, in under 100 ms, in the session's language and under `LANGUAGE=en_US:C`, on a machine with one root and no A/B boot state and with `/var` read-only. kinfocenter runs each on its GUI thread and stops System Settings with the page when one prints fewer than two lines | Preflight runs each script with an empty environment; `make aboutcheck` and `make check` run its `--selftest`; the full run is on a booted machine or with `--root` in a chroot |
| `testing/lnfcheck.py` | The KDOS global themes hold only the `[file][group] key` entries Plasma's look-and-feel manager reads, every font a theme applies is declared where the manager decides a theme has fonts, every theme has its splash, layout and previews, and every launcher a desk pins is a box the login warms | `make lnfcheck`, `make check`, with its `--self-test`; by hand, `--tree build/fs` reads the installed packages |

## selftest.sh

The self-test is the host-side regression suite for `src/libs/` and every C program built on it.

```sh
testing/selftest.sh
```

It compiles everything with the host compiler into a temporary directory, with
`-Wall -Wextra -Werror`, and runs each block in turn, printing a `==>` heading per block. The
blocks fall into these groups, in roughly this order:

1. **The libraries.** The shared assertion program, `src/libs/selftest.c`, is linked from the nine
   libraries (`libkbase`, `libkcolor`, `libkpkg`, `libkbuild`, `libktui`, `libkproc`, `libkxdg`,
   `libksig` with its vendored monocypher, and `libkpack`) and `kdos-portup`'s extractor, and run.
2. **The consumers.** `kinstall`, `kdos-appbox`, `kdos-theme`, `kdos-tools`, `kdos-kpkg`, the
   daemons, `kdosbuild`, `kdos-portup`, `kdos-packd` and `kdos-pack` are compiled against the
   libraries to prove the headers still agree with their callers, and several run their own
   `--selftest` against fixtures. `kinstall`'s plan resolves over the shipped catalogue and its
   answer file reads back what `--save` writes. `kdos-theme`'s identity kit is held to its floors:
   one KDE colour scheme per accent and the grey `KDOSBox` scheme, every scheme above its contrast
   floors (the view's text over a translucent selection and the quiet Window shades among them),
   with `paper`'s accent as its selection and a dark accent's selection carrying the text, the
   check proved to refuse a dark selection, a bright one, a vivid Window ground and a bright
   selection hover, one editor theme per accent carrying its scheme's view and selection
   backgrounds, the login screen's merged `kdeglobals` byte-stable, keeping the keys it does not own and carrying the scheme's
   contrast, every Konsole profile naming its scheme with ligatures off,
   every Konsole scheme carrying its 24 colours and every qt5ct palette its 21 roles per group, and
   every wallpaper package carrying a dark and a light picture at the sizes asked for.
3. **Boxes and packs.** Box profiles, and the pack store, its solve, its grafts and its signature
   checks.
4. **Packaging and the build.** The version checker against a recorded corpus, offline; the ports
   tree resolving through `kpkgdepends`; `kpkg` refusing a recipe whose `depends` outgrow its
   limit; the desktop entries of four game ports (`nethack`, `frotz`, `bsd-games` and
   `moon-buggy`), read from their build scripts, checked for their category, terminal flag and
   command; a package built twice being byte-identical; the package lint refusing a synthetic port
   for each rule (an unreadable file, a program with no `x` bit, a world-writable file, a `.orig`,
   a `usr/lib64` or `usr/etc` path, an `etc/X11` or `usr/lib/systemd` path, an empty `.pc`
   `Version`, a build-directory `RPATH`, an executable stack) and rolling one that holds a path
   from each allowlist, with the Python bytecode compiled into the package and `direct_url.json`
   left out; `kpkg` copying licences and pruning manuals and translations, and compressing cheaply
   only a package it deletes after installing; the shared indexes, file ownership, `kpkg check`
   finding missing, changed and unowned files while passing an edited `/etc`, skip-if-installed,
   the package store, `kpkg install --build-only` and `--commit`, the binary host's signed index
   and `kdos update` offering only what it covers, `kpkg fetch` taking a source from a local archive
   and telling an offline network from a missing file, and delta packages.
5. **The system programs.** Thumbnails landing in the shared cache; a terminal view noticing its
   host terminal hang up; timers and reminders; service supervision (a crash-looping service shown
   as `[FAIL]`, a running one supervised once, `stop` waiting out its grace period, a daemon never
   orphaned and sent one `SIGTERM`, `--restart-with`, `service list`); each user's service table
   and the backend through which `turnstile` starts the user services at the first login and stops
   them backwards; per-user timers; `kdos app tui`; `kdos-share`; the `-march` keep decision;
   `kdos rebuild`'s refusals; `kdos doctor`'s microcode check; the A/B slots: a bad update rolling
   itself back, each slot booting its own kernel, a UEFI candidate getting one boot through
   `BootNext`, a slot named by its version, `kdos update --arm` arming only a filled slot, a slot
   image signed, rebuilt from its delta and refused when tampered with, armed with the machine's
   own configuration, and offered and written into the inactive slot; `kdos-sysupdated` answering
   Discover over fixture slots; the initramfs's encrypted-root unlock; the shipped console palette
   matching the default scheme; the boot splash's two motions.
6. **The orchestrator.** `kdosbuild --selftest` laying out the build screen and its HUD at every
   size, retrying a failed phase over a three-phase tree, its jobserver and `timings.json`;
   `kdosbuild` running a synthetic build end to end, writing its snapshots as a full archive and
   layers that each restore exactly, running no step for a port installed and current, and building
   a phase by level under `--port-jobs`.
7. **The installer and the daemons.** `kinstall --dry-run` saying what it would do; `kinstall-core`'s
   JSON event stream, the answers every front end shares and the ones it refuses, the files it writes
   for a Plasma machine (SDDM's autologin, starting locked where asked, the global theme, the desk)
   and its A/B layout; the shipped polkit rules answering as the
   security model says; `kdos-powerd` letting the wheel group change the firewall, the accent and the
   splash and asking polkit for anyone else; `kdos-settingsd` keeping `hostname1`, `timedate1` and
   `locale1` in the files KDOS reads; `kdos-energyd` attributing energy to applications; the ports
   helper; `kdos-oomd` choosing the victim the desktop can afford to lose and never the Plasma
   session.
8. **Boxes in the session.** `kdos clone`; `genlaunchers` turning an image's entries into host
   commands and naming each launcher with the app id KWin gives its windows; a launch handing the
   box only its own sockets, or refusing; a throwaway box; `kdos-appbox open`.
9. **The `kdos` command in a Plasma session.** `kdos cve`; `kdos hey` and `kdos appid` reading
   KWin's window list; `kdos theme --audit`, and `kdos theme` in a session moving only what is
   KDOS's, with Phosphor mode putting every key back; the session verbs reaching Plasma's switches;
   `kdos backup run`; `kdos-sfx`; `kdos privacy`; `kdos places`; the checklist, persistence and
   speech verbs; `kdos help --keys`; the MPRIS bridge for mpd; the KDOS shortcut layer reaching a
   person once; the firewall prompt; the KRunner plugin's answers; `kdos-notice` saying each change
   once and confirming only a session that stays up; the About page's rows; the key card placing
   every shortcut KDOS ships; and the monospace gates. These run against stand-ins from
   `testing/fixtures/kdos-command`: a fake `kwriteconfig6`, a fake Plasma session bus, a fake
   compositor offering only the window list, a fake mpd and a fake `pw-dump`.

It needs no container and no network. On a bare development machine it takes a few minutes and
skips, by name, the blocks it lacks libraries or tools for (see
[What it does not cover](#what-it-does-not-cover)).

The suite keeps its own state directory: it sets `XDG_STATE_HOME` to a directory inside its
temporary output, so nothing it runs reads back what a previous run, or the developer's own
session, left behind, and nothing it runs can damage that session's state.

Its assertions are the libraries' invariants. Each states a property that was measured, and the
suite notices when it stops being true. One is counter-intuitive: the two colour-mixing functions
are asserted to *disagree*, because each generated file was written against exactly one of them.

A test whose only subject is code nothing ships is not coverage. It reports green while the
behaviour a person sees goes untested. When the last shipped caller of something goes, its
assertions go with it.

### A short TMPDIR

The suite makes its temporary directory with `mktemp -d`, under `$TMPDIR`, and several blocks bind
Unix sockets inside it. A socket path is at most 108 bytes, so a deep `TMPDIR` fails those blocks
with an error about the path, not about the test: the box launch block refuses with `the box name
is too long for a socket path`, and the installer holds a disk path of at most 63 bytes, which the
event-stream block keeps short by working relative to its own directory. A `TMPDIR` that is a
symbolic link fails the thumbnail block too, because the thumbnail's URI is made from the resolved
path. `/tmp` serves on most hosts; where it is small or deep, use a short real directory:

```sh
mkdir -p /dev/shm/st && TMPDIR=/dev/shm/st testing/selftest.sh; rm -rf /dev/shm/st
```

### Run it sanitized when you touch a parser

```sh
CC="cc -fsanitize=address,undefined -g" testing/selftest.sh
```

The suite must stay clean under both sanitizers. AddressSanitizer and UndefinedBehaviorSanitizer
catch what a plain run takes for a pass, for example a size field in an archive header overflowing
a signed type and becoming an enormous unsigned length, a copy called with a null source, or a read
past the end of a buffer that was last resized smaller than the loop assumes.

A variadic `printf`-style wrapper guards its own format string, in `libkbase` and in the two
programs that have one of their own (`kdos-kpkg` and `kdos-packd`). A null format is undefined in
`vfprintf` anyway, and the sanitizer build's whole-program analysis cannot prove one non-null, so
without the guard `-Wformat-overflow` refuses to build the suite and the sanitized run cannot be
done at all.

Leak checking is off by default (`ASAN_OPTIONS=detect_leaks=0`) and on for the library assertions.
Every program under test owns its parsed state until it exits, which a leak checker reports as a
leak and turns into a non-zero exit; the library suite is the one binary whose subject is code
called repeatedly inside a long-lived process.

### What it does not cover

Several blocks need libraries or tools a bare development machine does not have, and are skipped by
name when they are missing:

- the daemons' bus faces, the session verbs, the MPRIS bridge, the KRunner plugin, the notices and
  `kdos-sysupdated` need an sd-bus library (basu or libsystemd), `dbus-daemon`, `dbus-run-session`
  and, for some, `busctl`, `dbus-send`, `dbus-monitor` or Python's `dbus` module;
- `kdos-boxsock` needs `wayland-client` and the `security-context-v1` protocol data;
- `kdos-theme` needs the `libpng` headers;
- the polkit block needs `duk` or `node`; the MIME blocks need `shared-mime-info`; the microcode
  block needs `cpio`; the slot-image blocks need `curl`, `zstd` and `xz`; the monospace gates need
  fontTools; a slot arm needs root, and the firewall client check a user namespace.

A bare host still reaches every section and exits clean. What it loses is inside them, and each
block it cannot build says so.
[The machine where nothing is skipped](#the-machine-where-nothing-is-skipped) is the development
image that runs nearly all of them.

Every conditional block keeps one rule: a missing tool or library is a skip with a name, decided
before the block starts, so its absence cannot take the rest of the suite with it.

The terminal tools built on `libktui` compile here with format-truncation warnings disabled and
nothing else relaxed. Those programs truncate on purpose: every label goes into a fixed number of
cells, so a cut label is the intended behaviour. Where truncating *is* a defect, such as a socket
path or a device node, the code holds it with an explicit bound rather than relying on a warning
that cannot tell the two apart.

The Qt code is not here at all. It is `plasmatest.sh`'s.

### Reading pkgstore-check.log

A build with `KDOS_PKG_STORE=check` builds every port the [package
store](../03-architecture/packaging.md#check-mode) would have reused, and appends one stanza to
`build/logs/pkgstore-check.log` for each whose package differs from the stored one:

```text
<port> <key12> stored=<sha256> built=<sha256>
  - <mode>  <sha256 or link target>  <path>     the stored package's member
  + <mode>  <sha256 or link target>  <path>     the rebuilt one
```

Up to ten differing members follow each line, in the fingerprint `kpkg verify` uses. A member that
differs on every rebuild, such as an embedded date or a random seed, is a reproducibility defect in
the port; confirm it with `kpkg verify --repro <port>`. A port that differs only after a change
elsewhere read an input the key does not cover, usually an undeclared dependency. The port's own
log names each undeclared library link as `<port> links <owner> without declaring it`. An empty or
absent log means every hit rebuilt to the same bytes.

### The cached host helpers

The scripts under `ports/` compile helpers on first use and cache them: `ports/.kpkgbin` (the
recipe reader), and `ports/.portup` and `ports/.portup-tools` (the version checker). A binary built
against one C library cannot run under the other, so a host run straight after a container run
must delete them first.

The failure does not say so. The recipe reader fails, the version checker cannot render a
candidate, and the suite reports that it reproduced no outcome, which reads as a regression in the
version checker.

A container run as root also leaves those files owned by root, so deleting them as yourself fails
and the stale binary survives. Delete them from a container rather than with `sudo`.

## plasmatest.sh

`testing/plasmatest.sh` is the Qt half of the regression net. `selftest.sh` holds KDOS's C to a
bare host; this holds its Qt code, which no bare host can build. It runs where Qt 6, KDE Frameworks
6 and Plasma's development files are installed, the development image, and anywhere else it
refuses, naming what is missing, rather than skip and read as a pass:

```sh
testing/devdeps-image.sh plasmatest                    # every port, in the development image
testing/devdeps-image.sh plasmatest kcm-kdos-boxes     # one port and the ports it is generated by
make plasmatest PLASMATEST_ARGS=--selftest             # inside the image: the pipeline on its fixtures
```

The ports it builds are the desktop's: every `src/` port that is Qt code (its `CMakeLists.txt`
finds ECM, Qt 6 or a Framework) or ships Plasma packages (its source holds QML, a `metadata.json`
or an SDDM theme's `metadata.desktop`), and every `src/art` and `src/desktop` port those depend on,
which is where the generators of a look-and-feel, a colour scheme or a wallpaper live. A daemon or
a system tool they need only at run time is `selftest.sh`'s. It runs four stages, each a failure of
its own:

1. **Build and QtTest.** Each port is built by its own `build.sh`, with the one change that its
   CMake configure gets `-D BUILD_TESTING=ON`, and its tests run under `ctest
   --output-on-failure`: headless (`QT_QPA_PLATFORM=offscreen`), with a fresh home and runtime
   directory, and each test on a session bus of its own, so tests that serve fakes of the same D-Bus
   name run side by side. The tests live beside each port in `src/desktop/<port>/tests/`: the
   Discover backend parsing `kpkg`, pack and box output, the System Settings pages' models against
   fake `kdos` helpers, the KRunner plugin's matches, the notices' decisions.
2. **Plugins.** Every plugin a port installs under `lib/qt6/plugins` is loaded with
   `QPluginLoader`: a missing symbol, a library the image lacks or metadata with no `KPlugin.Name`
   is a plugin Plasma drops without a word.
3. **Packages.** Every Plasma package a port installs (each look-and-feel, desk, Plasma style,
   plasmoid, KWin script and effect) is installed with `kpackagetool6` into a scratch package root
   under its package structure; an SDDM theme is checked for the `metadata.desktop` and main script
   `sddm-greeter` loads, and copied into a scratch themes directory the way the Login Screen page
   installs one.
4. **qmllint.** Every QML file a port installs or compiles into its plugin is linted against the
   installed QML modules and the ones the ports just built. A context property (a KCM's `kcm`, the
   greeter's `sddm`) and a type a C++ plugin registers at run time cannot be resolved by reading the
   file, so unqualified access, unresolved types and unused imports are reported without failing; a
   module its host program registers in C++ with no `qmldir`, such as KWin's own, is let through by
   name; a file copied from an upstream package is upstream's. Everything else qmllint finds fails.
   The desk layout scripts are checked by `node`.

Naming ports runs stages 1 to 3 for those and the ports they are generated by, and stage 4 for their
QML. `--keep` leaves the scratch tree and prints where it is. `--selftest` runs the whole pipeline
on `testing/fixtures/plasmatest`, where every stage must pass its good case and fail its bad one: a
Qt port whose test passes and whose plugin loads, the same port with a failing `QCOMPARE` and a
plugin naming a symbol nothing defines, a configure that fails, a clean and a broken QML file, and
a plasmoid whose metadata has no id.

A KWin effect's QML needs KWin to run, and KWin's virtual backend needs a render device a container
does not have, so the effects are linted and installed here and seen working only on the rig.

## Fixtures

A **fixture** is recorded system state, or a stand-in for a peer, that a program can be pointed at
instead of the live machine. It is what makes a monitor, an attribution engine, a kill-selection
policy or a D-Bus service testable at all. The seam is the same everywhere: the process and system
filesystems sit behind a movable root, a variable moves one directory walk, or a fake takes the
real peer's bus name on a private bus. There are 30 fixture directories under `testing/fixtures/`.

| Fixture | Records | Makes testable |
|---|---|---|
| `res` | A process and system tree, and a second one (`next/`) taken later | `libkproc`'s readings, deterministically, including the rates that need two samples |
| `energy` | Four recorded power and process trees | The nesting rule, the counter wrap, the roll-up onto one application, and the short-lived residue |
| `oomd` | Trees arranged so only the memory budget produces the right answer, a Plasma session's processes, and a stand-in for logind's session lookup | That the budget check matters, that a stall with 80% of memory available kills nothing while the same tree at 7.5% does, and that the session's processes (`kwin_wayland`, `plasmashell`, `ksmserver`, `kded6`, `sddm` and the rest) are never chosen |
| `box` | Four box profiles | What a profile says it enforced and what it could not |
| `pack` | The metadata of six packs, not the packs | What `kdos-packd` would mount and what it refuses: which stack, which cannot, which is signed |
| `catalogue` | A small catalogue with every row type | The catalogue parser, in `kdos-appbox` and in the Discover backend's tests |
| `clone` | Three hand-built image headers | The two-record length rule |
| `cve` | Six ports and an eleven-row database | A version behind two fixes, one that only looks behind because of a packaging revision, a name mapping, a package the database has never heard of, and two ports on an older release branch |
| `portup` | Recorded upstream responses and, under `ports/`, the recipes seven ports were recorded at | Every discovery adapter and filter in `kdos-portup --selftest`, offline and unaffected by bumps to the live recipes |
| `recent` | A recently-used list | The recent-files reader |
| `polkit` | A rule harness, a stub of polkit's JavaScript environment and the upstream defaults, run under `duk` | What the shipped polkit rules grant an active local session, an inactive one and a wheel member, and what they must not |
| `settingsd` | A configuration tree, a keyboard layout list and a polkit stub | `kdos-settingsd`'s three services against the files KDOS keeps, on a private bus |
| `sysupdated` | Fixture slots, a client and a polkit stub, and a diff of its interface against Discover's | `kdos-sysupdated` answering Discover's `sysupdate1` calls, member for member |
| `kinstall` | A stand-in install target and `harness.c` | The files `kinstall-core` writes for a Plasma machine |
| `kdos-command` | A fake `kwriteconfig6`, a fake Plasma session bus with `timedate1`, `hostname1` and `locale1`, a fake compositor offering only the window list, a fake mpd, a fake `pw-dump` and a backup configuration | The `kdos` command's session verbs, without a session |
| `view` | `viewcheck.c` | `libktui`'s four list views |
| `plasmatest` | Qt ports, QML files and Plasma packages that pass and fail on purpose | `plasmatest.sh --selftest` |
| `releaseset`, `gearmeta` | Recipe trees holding each KDE family at one release, and every Gear application in one meta | That preflight's release-set and Gear-meta checks fail a split release, an application no meta names and one two metas name |
| `toolkit` | ELF cases built at run time | That the toolkit guard refuses what each area may not link |
| `colourlit` | A QML and a C++ file with literals marked | That the colour check reports every literal and only those |
| `chordcheck` | Shortcut sets that collide (Meta+Space bound to KRunner while fcitx5 keeps it, Meta+Q rebound while plasmashell keeps it, a forward switch moved onto its own backward one, a QML effect taking a chord) and a recorded `kglobalaccel` session | That `chordcheck.py` fails each, from files and live, and passes a clean set |
| `mimecheck` | Lists that are good, dangling and malformed, and a fixture tree | That `mimecheck.py` fails an id nothing installs and a list that does not parse |
| `defaultscheck` | `defaults.tsv`, one row per default | What `defaultscheck.py` checks |
| `kpackage`, `kwinrules`, `layoutjs`, `sddm`, `aboutdistro`, `units` | Good and bad cases for preflight's metadata, window-rule, desk-layout, greeter, About-page and unit checks | That each of those checks can fail |

To know a fixture tests something, disable the check it guards and watch it fail. The fixtures
added for a preflight check are run by the check itself first: a check that cannot fail its bad
case fails the run.

The fakes on a private bus are second processes on purpose. A protocol is a conversation between
two peers, and a mock compiled into either side passes on the mistakes only a real peer exposes.

## The machine where nothing is skipped

`selftest.sh` runs everywhere and skips what it cannot build, saying so each time, and
`plasmatest.sh` runs nowhere else. The *development image*, `kdos-devdeps`, is where both run
whole. It is not the *build container*, `os-dev`, which `make build` builds from the repository's
`Dockerfile` and runs the build in; that one carries the packaging toolchain but none of the
development libraries the two suites probe for.

```sh
testing/devdeps-image.sh                       # builds kdos-devdeps, then runs selftest.sh in it
testing/devdeps-image.sh plasmatest [ports]    # the Qt suite in it
testing/devdeps-image.sh -- <command…>         # anything else, with the checkout at /kdos
```

`testing/Dockerfile.devdeps` is Alpine edge, because the target is musl and a feature-test
difference is better met here than in a phase build, and because only edge carries Plasma 6.8, KDE
Frameworks 6.31 and Qt 6.11, the release set KDOS builds a point release behind at most. It has
three halves:

- **The C half**, exactly what `selftest.sh` probes for: `basu`, `libpng`, `sqlite3`,
  `wayland-client` and the `security-context-v1` protocol, `dbus-daemon` and its tools, `duk`,
  `node`, `cpio`, `xz`, `zstd`, `curl`, GNU `tar` and fontTools.
- **The Qt half**, what a KDOS Qt port's `CMakeLists.txt` asks for (Qt 6 with its private headers,
  every Framework a KCM, plasmoid, KWin plugin or the installer links, and `libplasma`, KWin,
  KDecoration, `plasma-workspace`, `libksysguard`, `layer-shell-qt` and Plasma Activities) and what
  `plasmatest.sh` runs: `ctest`, `qmllint`, `kpackagetool6` with every package structure Plasma
  installs, `clang-format`, `node` for the desk scripts, and the applications whose QML modules
  KDOS's QML imports (`plasma-welcome`, `plasma-desktop`, `sddm`).
- **The headless half**: Weston's headless backend with its desktop shell, `at-spi2-core`,
  `pyatspi` and the Python libraries selenium-webdriver-at-spi needs, for the AT-SPI dumps (see
  [Goldens](#goldens)), and Python's D-Bus and GObject bindings for the fake Plasma session.

**Discover is built from the tree's own release.** `libdiscover` installs no header upstream, and
every Discover plugin interface carries Discover's full version in its identifier, so
`kdos-discover` configures only against the exact release the tree ships. The image's last layer
builds that release from the same tarball, with the recipe's flags, and copies `libdiscover`'s
headers to `/usr/include/discover` as the recipe does; `testing/devdeps-image.sh` reads its version
and hash out of the `discover` recipe, so a Plasma bump rebuilds that layer and nothing else does.

Two of its packages are there for what they unblock, and their absence is reported as something
else: GNU `tar`, without which the reproducible-build block answers "the synthetic port did not
build" and stops the suite there, and `duktape`, without which the block that evaluates the shipped
polkit rules, which are the whole of KDOS's daemons' authorisation, skips. One block still skips
there: `busctl`, which Alpine ships in no package, gates the `kdos-powerd` and `kdos-sysupdated`
bus blocks.

## Running a shipped program without booting

`build/fs` is a complete musl root, so a program already installed there can be run directly, with
no ISO and no emulator, in seconds rather than minutes:

```sh
docker run --rm -v $PWD/build/fs:/rootfs -v /path/to/inputs:/rootfs/tmp/in:ro \
    alpine chroot /rootfs /bin/sh -c 'w3m -dump /tmp/in/page.html'
```

This checks a filter chain, a converter or any program that reads a file and writes text against
the binaries that ship rather than the host's. It has three limits:

- There is no `/proc` and no `/sys`. Anything that reads either behaves differently; `w3m`, for
  one, prints a garbage-collector warning here that it does not print on the machine.
- `unshare` is refused inside the chroot, so a program that probes for a namespace takes its
  fallback path. The fallback is easy to exercise and the namespace path impossible, and loosening
  the container does not help: `--privileged`, `seccomp=unconfined`, `apparmor=unconfined` and
  `--cap-add SYS_ADMIN` all still answer `Operation not permitted` for a `chroot`ed process.
- Nothing is supervised and no session exists. A program that wants `$XDG_RUNTIME_DIR`, a bus or a
  terminal needs the rig.

Everything the program writes stays in `build/fs`, and the ISO is built from `build/fs`. A bind
mount creates its own mountpoint, and a program run under the chroot writes to its `HOME`, `/tmp`
and wherever else it likes. None of that is owned by a package or by `fs/`, so neither the
[orphan sweep](build-system.md#sweeping-orphaned-packages) nor the
[`fs/` manifest](build-system.md#syncing-fs) removes it, and it ships. Bind inputs read-only under
`/tmp`, clean up afterwards, and run `testing/preflight.sh`: it refuses a `build/fs` whose top
level is not a root filesystem.

## The QEMU rig

The rig boots a real KDOS image in QEMU, drives it with keys, pointer and commands, and
photographs the screen. Use it for what nothing offline can show: the login, the session coming
up, a page as a person sees it, window management, a boxed application, and Phosphor mode, whose
CRT effect loads only on GPU rendering.

`testing/vnc-shot.py` is the driver. It boots `build/iso-build/kdos.iso` headless under UEFI
firmware, with the serial console and the QEMU monitor on Unix sockets, logs root in on the serial
console, and reads the framebuffer over the remote-framebuffer (VNC) protocol. Steps run in the
order they are given on the command line, so one boot can open a surface, photograph it, close it
and open the next.

```sh
testing/rig-image.sh          # builds kdos-qemu-py:latest; needs the network, once
docker run --rm --device /dev/kvm -v $PWD:/kdos -w /kdos kdos-qemu-py:latest \
    python3 testing/vnc-shot.py --audio --size 1920x1080 --dismiss-picker \
    --cmd 'systemsettings kcm_kdos_boxes' --sleep 4 --shot build/shots/x.png
```

The shot is written as raw PPM whatever its extension; convert it before opening it as a PNG.

### The readiness gate

`inittab` starts SDDM on `tty1` once `rcS` has returned. On the live medium,
`/etc/sddm.conf.d/kde_settings.conf` logs `kdos` in with no greeter; an installed disk shows the
greeter. Before any step runs, the rig passes a **readiness gate**, polled in the guest over the
serial console:

| `--until` | The gate waits for |
|---|---|
| `plasma` (the default) | `org.kde.plasmashell` owned on the user's session bus, `/run/user/<uid>/bus`, and KSplash at its last stage or gone |
| `greeter` | `sddm-greeter` and its KWin running as the `sddm` account, for a disk with no autologin |
| `shell` | Nothing past the serial root shell, for a run whose root script is the test; `--no-session` is the same |

A gate not passed within `--gate-timeout` seconds (300) fails the run, with the processes, SDDM's
log and the session log printed, instead of photographing a black screen. `--wait` seconds (4)
then let the splash fade and the panel finish laying out.

The first login opens Welcome Center with the desk picker over the desktop, so a run's first
`--shot` photographs the picker rather than what you asked for. `--dismiss-picker` closes it once
it opens; put it before the first `--shot`, as `testing/quick.sh` and `make shots` do.

`--keys`, `--chord` and `--type` reach the active virtual terminal, which is KWin's on `tty1`, so
they go to the focused window of the session. `--cmd` runs a command in the session: a login
shell of `--user` (`su - kdos -c`, so that podman resolves the home directory) with the running
plasmashell's environment, so the program it starts is on the session's bus and display, as it
would be from Kickoff. `--root-cmd` and `--root-script` run as root on the serial console, so
neither disturbs what is on screen. `--console-cmd` switches to `tty2` with Ctrl+Alt+F2 after the
gate, logs in there as `--console-login` and types its text; `--chord ctrl+alt+f1` returns to the
session.

### The fast loop

`make build` with packaging spends most of a small change's time writing the ISO, even with
`KDOS_ISO_COMP=zstd:3`. Repacking the whole medium to carry a 200 KB plugin makes every
look-at-it cycle wait on it. `testing/quick.sh` avoids that:

```sh
testing/quick.sh plasmoid-kdos-privacy -- --sleep 3 --shot /kdos/build/shots/x.png
```

It needs an ISO from one full build. It rebuilds the named ports into `build/fs` with no packaging
and no snapshot (`--phases 50_desktop --rebuild <ports>`), tars exactly the files those ports own
into `build/fix.tar`, and boots the ISO at 1280×800 with that tar on a raw disk. Once the
readiness gate has seen the Plasma session up and the desk picker is closed, `testing/quickpatch.sh`
unpacks the tar over the live medium's in-memory overlay and restarts what reads the files.
Everything after `--` is passed to `vnc-shot.py` and runs after the patch.

What is restarted follows from what the tar holds, one rule per kind of component. The machine is
never rebooted, because the patch lives only in the overlay:

| The tar holds | What happens |
|---|---|
| KWin itself, a KWin plugin, a decoration, a compiled effect, a QML effect or a QML script | `org.kde.KWin` `replace`: KWin exits with status 133, its wrapper starts the new binary, and the session's windows reconnect to it |
| A JavaScript effect (`kwin/effects/<id>`) | `unloadEffect`, then `loadEffect`, only when it was loaded |
| A JavaScript KWin script (`kwin/scripts/<id>`) | `unloadScript`, `loadScript` and run, only when it was loaded |
| `kded6` or a kded module | `kded6 --replace`, with the old `kded6`'s environment |
| A KRunner plugin | KRunner quits; the next query starts it again |
| plasmashell, a plasmoid, a Plasma package, `libplasma`, an `org.kde` QML module | plasmashell quits and is started again with its old environment |
| A System Settings page, a Discover backend, any other program | Its running instances end; the next launch is the new code |

**Only JavaScript reloads inside a running process.** A plugin library stays mapped once Qt has
loaded it, and loading the same path again hands back the old code; KWin's QML effects and scripts
share one QML engine, whose type cache answers a reload of the same file with the type it compiled
first. A reload of either would photograph the old code and call it the new, so both take a new
process. A JavaScript effect or script gets an engine of its own per load and does not.

| Variable | Does |
|---|---|
| `KDOS_QUICK_PHASES` | The phases to build (default `50_desktop`); widen it to the phase of a port outside the desktop, e.g. `43_toolkits,50_desktop` |
| `KDOS_QUICK_NOBUILD=1` | Reuse what is already in `build/fs` |
| `KDOS_QUICK_KEEP=1` | Restart nothing: right whenever the program under test is started fresh by the run (a System Settings page, Discover, the installer), which then runs the new code |
| `KDOS_QUICK_RESTART` | `auto` (the default, from the tar), `none`, or a list of `kwin`, `plasmashell`, `kded` and `krunner` to restart exactly those |
| `KDOS_QUICK_FILES` | Extra paths under `build/fs` to carry, such as a default under `/usr/share/kdos/xdg` |
| `KDOS_QUICK_SKEL=1` | Copy `/etc/skel/.config` over the live user's configuration in the guest |

The file list comes from the package database: `build/fs/var/lib/kpkg/db/<port>` is what that
port installed, so a component that gains a new file is carried without anyone adding it.
`KDOS_QUICK_FILES` carries a file from the tree's `fs/`: those paths are installed into `build/fs`
by the file-system step and belong to no package, so the database does not list them. Add
`KDOS_QUICK_SKEL=1` for a file under `/etc/skel`, because skel seeds a *new* account and the live
user's home was seeded at boot.

What the loop cannot carry:

- A new port, anything under `fs/` that the file-system step has not installed, a kernel change or
  an initramfs change. Use the real build.
- An SDDM theme. The live session is already logged in, and no greeter runs to read it; log out,
  or boot an installed disk.
- Anything that owns a D-Bus name on the session bus. A restarted owner races its old self for the
  name and clients keep talking to the old one, so a daemon or a notification server is checked on
  a real boot.
- Evidence about the shipped image. What `make build` writes is the ISO; this is the loop to
  iterate in *before* taking the photograph that counts.

### The flags

| Flag | Does |
|---|---|
| `--shot <file>` | Photograph the screen now, into this path |
| `--out <file>` | Where the final shot goes when no `--shot` was given (default `/tmp/kdos-shot.ppm`) |
| `--keys <keys>` | A monitor `sendkey`, e.g. `meta_l` |
| `--chord <keys>` | Hold the modifiers and tap the key over VNC, e.g. `meta+shift+t` or `ctrl+alt+f2`. Use it for anything with two modifiers: `sendkey` presses and releases the whole combination at once, and such a chord never arrives |
| `--type <text>` | Type into whatever has the focus, then Return: the greeter's password field, KRunner, a Konsole the run opened |
| `--text <text>` | Type and stop, with no Return, for a search field or filter where Return would act on the first match |
| `--mouse X,Y` | Move the pointer, in absolute pixels |
| `--click X,Y[,BTN]` | Move there and click; `BTN` is 1, 2 or 3 |
| `--drag X1,Y1,X2,Y2[,BTN]` | Press at the first point, move to the second with the button held, release |
| `--press X,Y[,BTN]`, `--release [X,Y]` | A drag held open across later steps, so a `--shot` between two `--press`es photographs it in progress. A button stays down only while its VNC connection is open, so the whole gesture uses one connection, which is why this is two flags and not a longer `--drag` |
| `--sweep X1,Y1,X2,Y2[,N[,MS[,BTN]]]` | A drag at a real mouse's report rate: `N` interpolated motions `MS` milliseconds apart (default 120, 2 and button 1). `--drag` and `--press` space events 150 ms apart, so a defect that needs a fast hand needs this |
| `--sleep <s>` | Wait this many seconds before the next step |
| `--dismiss-picker [s]` | Close the first-run Welcome Center window once it opens, waiting up to `s` seconds (30) |
| `--until plasma\|greeter\|shell`, `--no-session` | What the readiness gate waits for; see [The readiness gate](#the-readiness-gate) |
| `--gate-timeout <s>` | Seconds from the serial shell to a passed gate before the run fails (default 300) |
| `--user <name>` | The session's account, for the gate, `--cmd` and `--dismiss-picker` (default `kdos`) |
| `--wait <s>` | Seconds to let the screen settle once the gate has passed (default 4) |
| `--soak <s>` | Let the session run this many seconds after the last step, before the final `--out` picture (taken only when no `--shot` step ran) (default 0) |
| `--cmd <cmd>` | Start a command in the background in the session, as `--user` with plasmashell's environment; nothing waits for it to finish |
| `--root-cmd <cmd>` | Run a command as root on the serial console, wait for it to finish and print its output |
| `--root-script '<file> [args]'` | Send a local script into the guest and run it as root with those arguments. A script that exits non-zero fails the run, with status 3, once every step has run |
| `--script-timeout <s>` | How long a `--root-script` or a `--root-cmd` may take (default 900) |
| `--console-cmd <text>`, `--console-login USER[:PW]` | Switch to `tty2` after the gate, log in there (default `kdos:kdos`, the live medium's) and type this, before any step |
| `--size WxH` | The guest's screen (default `1280x800`, virtio-gpu's own default) |
| `--gl` | `virtio-vga-gl` on a headless EGL display, so KWin composites with OpenGL on the host GPU through virgl and Phosphor mode can load. Needs `/dev/dri` passed into the container. No Vulkan |
| `--venus` | With `--gl`, also publish a Vulkan capability set. Does not work on an NVIDIA host; see below |
| `--audio` | Give the guest an HDA controller, so audio paths reach a real device: backed by the host's sound server when `testing/qemu-audio.sh` finds one, by a silent backend otherwise |
| `--disk <qcow2>` | Attach a virtio disk, the target an unattended `kinstall` writes to. With the ISO attached (an install run) the run refuses, before it boots, a qcow2 whose virtual size less what it already holds is more than the host has free |
| `--allow-disk-growth` | Run that install even when the qcow2 could outgrow the host's free space |
| `--no-cdrom` | Leave the ISO off, so the disk is what boots |
| `--root-password <pw>` | What to answer when the serial console's login asks root for a password, as an installed system does; the live image asks none (default `kdos`, the one `install-to-disk.sh` sets) |
| `--boot-disk` | Boot the disk with the ISO still attached, so an installed system can still reach the medium. `appsweep.sh` reads packs from `/mnt/iso/packs`, which only a medium you have put packs into carries; the shipped ISO has none |
| `--data-disk <file>` | Attach a raw file as a plain virtio disk, to carry a large file into a guest with no network. Input only |
| `--scratch <file>` | Attach a raw file the guest writes a tar onto, which is how files come back out |
| `--usb <file>` | Attach a raw disk image as a removable USB 3 stick, on its own xHCI controller |
| `--keep` | Leave the emulator running after the steps |
| `--serial-log <file>` | Where the serial console is logged (default `/tmp/kdos-serial.log`) |
| `--vnc-port <n>` | The VNC port (default 5909) |

Several of those answer questions a screenshot alone cannot.

`--audio` gives the guest a sound device. Without it PipeWire finds no card and every audio path in
the guest is untestable. The rig asks `testing/qemu-audio.sh`, the same probe every `make run*`
target uses, for a real host backend, and falls back to `-audiodev none` when it finds none. The
rig image carries neither PipeWire nor PulseAudio, so it gets the null backend. A null backend
consumes samples on a timer and a real one consumes them as a device does, so a guest whose timing
follows its own playback runs to a different clock on the two. To reproduce what `make run-hw`
does, use the accelerated image (`testing/qemu-hw`), which carries PipeWire. The device is
playback-only (`hda-output`), so recording is tested through `snd-aloop` instead; see below.

`--soak` lets the session run on after the last step and before the final picture, because a
program's cost over its first few seconds is its start-up, which is not what is being measured. A
`--shot` or `--root-cmd` step runs before the soak, not after it.

`--root-script` is the form for a check too long to be one command. The script travels
base64-encoded, in short lines, because a terminal in canonical mode silently drops everything past
its line limit and an encoded payload contains nothing the shell acts on before decoding. The rig
then waits for a marker the *guest* prints, so a step taking minutes is waited for rather than cut
off. `--root-cmd` waits on a marker the same way, up to the same `--script-timeout`, and prints
what the command wrote; it is the form for anything that fits on one line. Only `--cmd` is fire and
forget.

### Checking the session

`testing/plasma-probe.sh` asks whether the Plasma session on a booted KDOS is whole. It runs in the
guest as root, through `--root-script`, once the gate has passed; `make session-probe` runs it after
`session-cost.sh` on one boot.

```sh
python3 testing/vnc-shot.py --root-script 'testing/plasma-probe.sh --desk kdos'
```

Every check prints one line, `ok` or `FAIL` and what it looked at, and the exit status is the
number of failed checks, so 0 is a whole session and `vnc-shot.py` fails the run on anything else.
Between the checks it prints what a reader needs to act on a failure: every well-known name on the
session bus, KWin's loaded effects, the renderer, logind's view of the session, the panels and the
windows. It checks:

- the session's processes: `kwin_wayland`, `plasmashell`, `ksmserver`, `kded6`,
  `xdg-desktop-portal-kde` and the polkit agent;
- the names on the session bus, each asked with a `Ping`: `org.kde.KWin`, `org.kde.plasmashell`,
  `org.freedesktop.Notifications`, `org.kde.StatusNotifierWatcher` and
  `org.freedesktop.portal.Desktop`, and every `io.kdos.*` or `org.kdos.*` name the bus holds,
  which must answer; and `org.freedesktop.login1` on the system bus;
- logind's view of the session: `loginctl show-session` gives `Active=yes`, `Type=wayland` and
  `Seat=seat0`, and KWin's log has no `Failed to take control of` line;
- the desk: plasmashell's panels match the desk named by `--desk` (`kdos`, `dock` or `tiling`), or
  the account's own choice in `~/.config/kdosrc`, unless `--no-desk`;
- Phosphor mode: `kdos_crt` is a loaded effect with `--phosphor`, and is not without it;
- windows, with `--window ERE` (a window line must match) and `--box ID` (a window of that box is
  mapped and tagged as the box, and the portal's accent inside the box is the session's).

The session bus admits only the account that owns it, so every call on it runs under `setpriv`
with the account's ids, which opens no PAM session of its own. Windows come from KWin itself: a KWin
script lists `workspace.windowList()` and sends each window to a bus name nobody owns,
`io.kdos.RigProbe`, and the failed call reaches a `dbus-monitor` first, which is how the lines get
out of the compositor with no service of the probe's own. The sandbox fields of a window line are
the window properties KDOS's KWin patch adds.

### The login flows

`testing/flows/` holds seven scripts, each one boot of the rig driving one login path and
asserting every state it reaches. Flows 01 and 04 to 07 have run on the rig against the live
ISO: 01, 04, 06 and 07 pass, and 05 fails its lit-screen check because the screen does not come
back after resume (see
[Known gaps](../06-reference/known-gaps.md#the-suspend-flow-finds-no-screen-after-resume)). Flows
02 and 03 need an installed disk and have not run. Every flow is syntax-checked (`bash -n` in
preflight, and ShellCheck).

```sh
testing/flows/run.sh                 # all seven, one boot after another
testing/flows/run.sh 04 06           # the lock and the VT flows
testing/flows/04-lock-unlock.sh      # one flow
```

| Flow | Boots | Drives | Asserts |
|---|---|---|---|
| `01-live-autologin.sh` | the ISO | nothing | the session was opened by the `sddm-autologin` PAM service, is Active, Wayland and class `user` on seat0, and `plasma-probe.sh` passes |
| `02-greeter-password.sh` | the installed disk | the password typed at the greeter | the greeter is the active session with no session of the account; then a session opened by the `sddm` PAM service, and `plasma-probe.sh` passes |
| `03-logout-greeter.sh` | the installed disk | a password login, then `org.kde.Shutdown logout` | plasmashell exits and the greeter is the active session again |
| `04-lock-unlock.sh` | the ISO | Meta+L, then Shift and the password | `org.freedesktop.ScreenSaver` `GetActive`, logind's `LockedHint` and `kscreenlocker_greet` agree the screen is locked, then that it is not |
| `05-suspend-resume.sh` | the ISO | `loginctl suspend` with the RTC armed to wake the machine, then a pointer move | the kernel's `suspend_stats/success` moved, the session came back locked, the password unlocks it, `plasma-probe.sh` passes, and `flow_lit` finds both shots lit |
| `06-vt-switch.sh` | the ISO | Ctrl+Alt+F2, then Ctrl+Alt+F1 | tty2 is in front with its getty and the session inactive; then the session's VT is in front and the session Active |
| `07-no-tty-leak.sh` | the ISO | `touch /root/kdos-flow-leak` and Return, typed with no window focused | the file does not exist, the session VT's keyboard is in `K_OFF` mode and its tty holds no unread input |

Each flow is one `vnc-shot.py` run whose steps alternate input with `--root-script
testing/flows/probe.sh CHECK`, so a check sees the state the step before it left. The probe asks
logind (which session is active, its class, the PAM service that opened it, its VT, its lock hint),
the account's session bus (the shell's name, the screen saver) and the kernel (the foreground VT,
the VT keyboard mode through `KDGKBMODE`, the unread bytes through `FIONREAD`, the suspend counter).
All three report a healthy session while QEMU's scanout is inactive, so a flow that sets `FLOW_LIT`
also has those shots read by `flow_lit` in `testing/flows/lib.sh` once every probe has passed: a
frame whose 64 by 36 sample grid holds fewer than 16 colours, or one colour on more than 98% of
its points, is unlit, and the flow fails with status 4 naming it. QEMU's *Display output is not
active* frame and a black screen are unlit; the lock screen and the desktop hold about a hundred
colours. Only flow 05 sets it, after moving the pointer to wake the output; a text console is unlit
by this measure. The other shots, in `build/shots/flows/<flow>/` with the serial log, are for the
person reading a failure.

The two greeter flows boot a throwaway qcow2 overlay of an installed disk, `build/kdos-target.qcow2`
by default (`KDOS_FLOW_DISK`), made by `testing/install-to-disk.sh` with its default answers, which
set no autologin. Logout is tested there because the live medium's autologin sets `Relogin=true`
and logs straight back in. The password is `KDOS_FLOW_PASSWORD`, `kdos` by default, which is both
the live medium's and the installed disk's. The VT flow presses Ctrl+Alt+F1 for the session's VT
(`KDOS_FLOW_SESSION_VT` names another) and prints the VT logind records for the session, which is
the number to set when they differ.

The leak flow types a path under `/root` because typing on the desktop opens KRunner, whose command
runner runs the line as the account: a path the account may write would be created by KRunner and
tell nothing about a tty. The suspend flow needs QEMU's S3 and the firmware's resume path; when the
kernel offers no `mem` sleep state the probe fails saying so.

### Time to the panel, and memory at idle

`testing/session-cost.sh` boots the ISO on the rig, measures from inside the guest, and prints a
block of `session-cost:` lines, also written to `build/session-cost.txt`. It runs on the host and
starts the rig container itself; `--gl` boots with `virtio-vga-gl`, the configuration the timing
budget is set for, and passes the GPU into the container.

Times are seconds since the kernel started, read from each process's start time in
`/proc/<pid>/stat`, which is exact however late it is read: the display manager, the greeter, the
session, KWin, the shell. Two events leave no process behind and are timed by watching, every
quarter second from the moment the serial console has a shell: KSplash's last stage (seen by
`dbus-monitor`) and `org.kde.plasmashell` becoming owned. Where the watch began after an event, its
line says `<=`. Memory is the proportional set size (PSS) from `/proc/<pid>/smaps_rollup`, read
`--settle` seconds (60) after the panel is up, summed over the session's processes and over the
whole system, with the fifteen largest listed. PSS divides each shared page among the processes
mapping it, so a library shared by fifty processes is counted once.

The numbers for each release are recorded here once they are measured on a release image. No release's
numbers are recorded.

### Phosphor mode in the rig

A plain virtual display puts KWin on llvmpipe, which it treats as software rendering, and
Phosphor mode's CRT effect refuses software rendering, so `kdos phosphor on` is refused there and
nothing of the effect is in a photograph. Use `--gl`, with the GPU passed into the container
(`--gpus all --device /dev/dri`, as `session-cost.sh --gl` passes them), or the accelerated image.
The rig image carries QEMU's OpenGL module and `virglrenderer` for it.

### Vulkan in the rig

`--gl` alone starts `virtio-vga-gl` with no Vulkan capability set, so the guest's virtio driver
opens nothing and every Vulkan tool silently falls back to lavapipe, the CPU rasteriser.
`vulkaninfo --summary` naming `llvmpipe` with `PHYSICAL_DEVICE_TYPE_CPU` is the sign, and a `vkcube` or
`vkgears` rate taken that way is a number about the host's cores. `--venus` adds `venus=true` and
blob resources, which requires the guest's memory to come from a shared `memfd`, so it also changes
the machine's memory backing. On an NVIDIA host neither end works: with `--gpus all` (the only way
the container has a host Vulkan device to forward to) QEMU aborts during guest boot with exit 134;
without it the guest boots, the virtio driver loads, and instance creation fails with
`ERROR_OUT_OF_HOST_MEMORY`, taking lavapipe with it. `--venus` therefore stays opt-in, and a Vulkan
number from this rig is lavapipe's until `vulkaninfo` names a GPU.

### Driving the accelerated image

The accelerated image (`testing/qemu-hw`) needs no Python of its own. Start its emulator with the
serial and monitor devices on Unix sockets in a bind-mounted directory, `chmod 666` them from inside
once QEMU has made them (the container runs as root and the driver does not), and drive it from the
host by importing `vnc-shot.py`'s own `Serial` and `Monitor`. Meta+Return opens Konsole in the
session, so `Monitor.type()` is enough to start a program.

### How the rig reads the screen

A monitor `screendump` answers "no surface" under accelerated graphics, so the framebuffer is read
over VNC instead. VNC's encoding negotiation has an exact field layout, and one extra padding field
desynchronises the stream so every later read blocks forever.

### A real capture device, with no emulator flag

The emulated codec has no input, so nothing `--audio` gives can produce a capture signal.
`snd-aloop` can, and it is already on the image (`CONFIG_SND_ALOOP=m` in the kernel configuration,
with modules compressed with zstd). Loading it gives the guest a card named `Loopback` with two
PCMs, each with eight playback and eight capture subdevices, cross-wired: what is written to
`hw:Loopback,0` is read from `hw:Loopback,1`.

```sh
modprobe snd-aloop
sox -n -r 16000 -c 1 -t alsa hw:Loopback,0 synth 8 sine 200-2000 vol 0.5 &
sleep 2
sox -q -t alsa hw:Loopback,1 -t raw -e signed -b 16 -c 1 -r 16000 /tmp/cap.raw trim 0 3
sync
```

Run it as a `--root-script`, with nothing else holding the loopback's capture side.

- Use a swept tone. Silence and a flat signal pass identically whether a level is computed from the
  samples or hard-coded to zero; a sweep does not. The three seconds above come back as exactly
  96,000 bytes peaking at 16382, which is half of full scale, the `vol 0.5` that was played.
- The rates need not match. The player negotiates 48 kHz and the capture asks for 16 kHz signed
  16-bit; ALSA's plug layer converts, `sox` warns that it cannot use the format natively, and the
  bytes that arrive are correct. What matters is that the player opens first, so the capture side
  has a stream to read.
- End with `sync`; see [Harness traps](#harness-traps).

### Moving a large file into the guest

Use a plain virtual disk (`--data-disk`), not a USB device: it is not removable, so the session's
removable-media handling has nothing to act on. Use `--usb` only when the test needs a removable
device, which is the removable-media daemon's subject, or a stick to boot from. It attaches the
image as a USB 3 stick on its own xHCI controller, because the machine's default USB controller is
USB 1.1: a session booted off a stick on it reads its image so slowly that the login outruns
turnstile's `login_timeout` and Plasma never starts.

A file is read back by block copy and truncated to its exact length, because a raw drive is rounded
up to a sector boundary.

### The host may not have an emulator

The rig runs unmodified inside a container image that carries one, with the repository bind
mounted and hardware virtualisation passed through. The image is built from this tree:

```sh
testing/rig-image.sh          # builds kdos-qemu-py:latest from testing/Dockerfile.qemu
```

It needs the network once; everything afterwards runs offline against the ISO. The image is Debian
with QEMU, its disk utilities, the OVMF firmware an EFI boot needs, QEMU's OpenGL module with
`virglrenderer` and an EGL stack for `--gl`, and python3, and nothing else, because `vnc-shot.py`
speaks VNC using only the standard library. There is no pip step, so the only versions that matter
are the distribution's QEMU and OVMF.

Because the image is built from the tree, a photograph is reproducible from any clone.

### Harness traps

The following behaviours of the rig and the guest cause false results.

- Driving the session means a *login* shell (`su - kdos -c`). A plain user switch leaves the
  container tooling resolving the home directory to `/`, and every call fails on a permission
  error. A command that talks to the session also needs its bus, `DBUS_SESSION_BUS_ADDRESS`
  (`unix:path=/run/user/1000/bus`), which `--cmd` takes from plasmashell.
- A plain virtual display puts KWin on software rendering, so Phosphor mode is refused and not in
  the photograph. Use `--gl`, or the accelerated image.
- The first login opens Welcome Center's desk picker over the desktop. Dismiss it
  (`--dismiss-picker`) before any other shot.
- `pkill -f PATTERN` also matches any shell loop whose own command line contains `PATTERN`, such as
  a `while pgrep -f PATTERN` wait loop, so it kills the monitoring loop itself along with the
  target.
- An exact-name kill cannot match a process name past 15 characters, so a restart that uses one
  restarts nothing and the old program keeps answering. `xdg-desktop-portal-kde` and
  `polkit-kde-authentication-agent-1` are past it.
- The harness stops the emulator without a shutdown, so a root script must `sync` after writing,
  or a file it wrote comes back zero-length, whether it is read out of the guest or found on the
  next boot.
- A supervised daemon's output is not on the pipe that started it. `service start x | cat` returns
  as soon as the script does, with the script's own lines only; the daemon's are in
  `/run/kdos-svc.<name>.log` and syslog.
- A static screen produces no frames, so anything about dropped frames needs something animating
  first.
- A step costs seconds, so anything with a timeout must be photographed with no sleep before it.
  Typing goes one character at a time and a `--shot` is a full framebuffer over VNC: four shots take
  about a minute. With a five-second notification, a pulse or a tooltip's own delay, a `--sleep`
  before the shot photographs the desktop after the thing has gone, and the picture looks exactly
  like the feature being broken.
- A `--root-cmd` or `--root-script` round trip outlasts a five-second notification. It runs over the
  serial console and waits for the command to finish, which takes longer than the notification it
  raised stays on screen. Raise one with `--type` or a chord, which go over the keyboard and are
  quick, or ask for a timeout longer than the rest of the run.

## Goldens

A **golden** is the committed reference a check compares against. KDOS's goldens are the
accessibility trees of its own surfaces, in `testing/atspi/goldens`: every KDOS System Settings
page, every KDOS page of Welcome Center, the installer and the KWin pages KDOS configures, each
described by a file in `testing/atspi/surfaces`. A tree is read the way Orca reads it, over AT-SPI2,
so one golden is both a regression check and an accessibility gate.

### What a dump is

A dump is the tree as text, one node per line, indented by depth, so two runs of one build print
the same bytes and a change to a surface shows up as a line diff anyone can read:

```text
# kdos-atspi 1
# surface: kcm_kdos_boxes
application "kcmshell6"
  frame "Boxes" [enabled, sensitive]
    button "Apply" [enabled, focusable, sensitive]
    check_box "Allow network" [checkable, enabled, focusable, sensitive]
```

A line is `<role> <name> [desc=<description>] [<states>]`, with the name and description as JSON
strings. The role is AT-SPI's role name with spaces as underscores, except that a push button is
written `button`, because `at-spi2-core` names that role two ways depending on its version and a
golden must not change with the bus library. Only the states that describe the control are kept
(checked, editable, expandable, required); the ones that describe the moment (focused, active,
showing, armed, hovered) change between two identical runs and are dropped. A surface file may mask
a name that changes by itself, a clock or a size, which is then written `"<masked>"`, and may give
`steps` (clicks, typing, waits) that reach a page behind a button, which then has its own golden.

The tree is read one of two ways. With `--webdriver` the surface is launched and driven by KDE's
selenium-webdriver-at-spi, and its page source is what is dumped; without it the tree is read over
the bus directly through Atspi's GObject binding and written into the same XML first, so both routes
print identical dumps. The goldens are written from the direct route: the WebDriver's click runs a
control's "Press" action, and a Qt Quick check box answers Press without toggling, so a step that
toggles a check box, a switch or a toggle button behaves only on the direct route.

### The gate

`atspi.py check` compares each dump with its golden and fails on any difference. It also fails a
dump in which a visible control (a button, a check box, an entry, a menu item, a tab, a slider, a
list item) has no accessible name, because Orca reads nothing for it, and a dump that holds a
host's error page instead of the surface (kcmshell6's "Could not find plugin", a QML file that did
not compile), so a surface that did not load can never become its own golden. A surface may exempt
roles it does not own, never a control KDOS wrote.

```sh
make atspi                          # boot the ISO, dump every surface, check them
make atspi ATSPI_ARGS=--update      # write the goldens from the shipped image
make atspi-selftest                 # the dump format and the gate on fixtures; no ISO
```

`testing/atspi/golden.sh` boots the ISO, carries the harness in as a tar on a data disk, dumps each
surface inside the session with `testing/atspi/guest.sh` (as the session's account, with
plasmashell's environment, after telling the accessibility bus to switch on), brings the dumps back
on a scratch disk to `build/atspi/dumps`, and runs the check.

### Regenerating

Goldens describe the shipped image, so only `golden.sh --update` writes them, from a booted ISO,
and it refuses a dump that fails the gate. Read `git diff testing/atspi/goldens` before committing:
every changed line is a control a person meets differently. A dump with no golden fails, `no golden`, with the command
that writes it. The goldens directory is empty until an image has been through
`make atspi ATSPI_ARGS=--update`, so until then `make atspi` fails every surface that way.

`testing/atspi/headless.sh` dumps surfaces in the development image with no virtual machine, a
minute's loop instead of a boot. It starts a session bus, the accessibility bus and Weston's
headless backend, which renders with pixman and needs no render device, where KWin's virtual
backend would quit the first time it drew its own QML. `--prefix DIR` puts an install root first,
so a KCM built by `plasmatest.sh --keep` can be dumped; `--runs N` dumps every surface N times and
fails unless every run printed the same bytes, which is how a surface is shown to be deterministic
before its golden is trusted. It never writes a golden: the image carries Alpine's KDE build, a
point release from KDOS's at times, and a page that asks KWin itself over D-Bus sees Weston, so a
difference there is either the change under test or the two builds disagreeing, and a `golden.sh`
run says which.

## Checking the sources

Upstream source files are not in git. `make fetch` resolves each one from the local source cache,
the project's source archive or upstream, verifies it against the recipe's `sha256 =` line, and
puts it in its port directory (see
[Where sources come from](developing.md#where-sources-come-from)). Three checks verify it:

| Check | Run | Answers |
|---|---|---|
| `make fetch-check` (`ports/fetch --check [port…]`) | Offline, about a minute | Every hashed source that git does not track is on disk and matches its hash; it lists what is missing or corrupt and exits 1 if anything is |
| Preflight's source checks | `testing/preflight.sh` | No recipe-hashed archive is tracked by git, `.gitignore` keeps fetched sources and the cache out of commits, the fetch and publish scripts parse, and `ports/sources.idx` is a well-formed format-2 index |
| The pre-push hook | Enabled with `git config core.hooksPath script/hooks` | Offline first, a pushed tip whose ports layout breaks is refused: a recipe not exactly at `ports/core/<shelf>/<name>/`, a shelf missing from `ports/shelves` or named `libs`, `core` or after a port, or a bare name held twice across `ports/core` and `src/<area>/` (`KDOS_SKIP_LAYOUT_CHECK=1` skips it). Next, a new or changed `.patch` or `.diff` under `ports/` or `src/` without a `Subject:` line is refused (`KDOS_SKIP_PATCH_CHECK=1` skips it). Then `testing/docscheck.sh` and `testing/kfscheck.py` run over the working copy, not the pushed commit, so push from a checkout of what is pushed (`KDOS_SKIP_BOOK_CHECK=1` skips them). Last, a push naming a source hash that `ports/sources.idx` and the archive do not hold, or one of whose parts the archive lacks, is refused, as is one made while the archive is unreachable (`KDOS_SKIP_PUBLISH_CHECK=1` skips that check only); see [Writing ports](writing-ports.md#the-pre-push-hook) |

A file git tracks, such as a patch or a configuration file beside a recipe, is never treated as an
archived source: `ports/fetch` and `--check` skip it. A recipe-hashed archive that git tracks fails
preflight with "recipe-hashed archives are tracked by git — git rm --cached them", and
`make fetch-check` reports it as neither present nor missing. A successful check ends with
`All N archived sources present and verified`; a count of 0 there means nothing was checked.

## docscheck.sh

`testing/docscheck.sh` is the authoring check for both books, this one under `docs/kdos` and
*KDOS from Scratch* under `docs/kfs`.

```sh
bash testing/docscheck.sh                                     # both books, plus README.md
bash testing/docscheck.sh --book docs/kfs                     # one book (repeatable)
bash testing/docscheck.sh docs/kdos/05-developer/testing.md   # only the named pages
```

It reports:

| Report | Means |
|---|---|
| `DEADLINK` | A relative link whose target file exists neither beside the page nor from the repository root. The part after `#` is not checked; [anchorcheck.py](#anchorcheckpy) checks it |
| `HISTORY` | A line containing a phrase from the list in the script's `BAD` variable, phrases that almost always record the past rather than state the present. At most five lines are printed per page |
| `NOTITLE` | A page of either book whose first line is not a `# ` title |
| `NOSEEALSO` | A page of either book with no `## See also` section |
| `UNLISTED` | A page its own book's `README.md` does not link (whole-book runs only). In `docs/kfs` the contents is written by `testing/booknav.py --write`, so an unlisted page there is one booknav has not been run over |
| `WIKISKEL` | A topic page of *KDOS from Scratch*'s system administration part (the `*-wiki` directory, every page but its `intro.md`) whose `## ` headings are not exactly Installation, Configuration, Usage, Tips, Troubleshooting, Known gaps and See also, in that order. Deeper headings are free |
| `MISSING` | A path given on the command line that does not exist |
| `BOOKCOUNT` | A port count on one of the three pages that state exact counts, or a row of the ports catalogue, that the tree contradicts (whole-book runs that include `docs/kdos` only; see below) |

A whole-book run that includes `docs/kdos` also runs `testing/bookcounts.py`. Exact counts of the ports tree are written on three
pages only: [Status](../06-reference/status.md), [The ports
catalogue](../06-reference/ports-catalogue.md) and [Packaging](../03-architecture/packaging.md).
Every other page says "about 2,000" or links to one of them. The script recomputes each figure those
pages state: the recipes in `ports/core` and in each `src/` area, the shelves, the list lines and
distinct names, each phase's names, installs and `packages.d/` files (resolved over `depends =` as
`phaseclosure.py` resolves them), and each shelf's count. It checks that the catalogue has every
recipe exactly once, under its shelf or area, with the recipe's version and the phase that installs
it; that its list of ports named by more than one phase is complete; and that no port in its
application-developer platform table has gained a consumer. It also fails on a recipe no phase
installs that `ports/unbuilt` does not name, on a name there that a phase does install or that has
no recipe, and on a catalogue row for such a recipe whose phase is not `not built`. A figure is
found by the sentence or table row that states it, so rewording one of those sentences makes the
check report it as not found until the pattern in the script is changed with it.

It prints `docscheck: ok` and exits 0 when nothing is reported, and exits 1 otherwise. It cannot
catch a paragraph that narrates history in its own words.

## anchorcheck.py

`python3 testing/anchorcheck.py` checks the part of a link after `#`, in every page of both books
and in `README.md` and `CLAUDE.md` (`--book DIR` narrows it, a path checks one page). The fragment must be an id
the target page has as GitHub assigns it: the heading's rendered text lowercased, every character
other than a letter, digit, `_`, `-` or space dropped, each space turned into `-`, and a repeated id
suffixed `-1`, `-2` in page order. An `<a id="…">` counts too; a heading inside a code fence does
not. The rule is the one `script/book/kfsgen.py` and `kdos-doc`'s `mkbook.py` give their headings,
so the three change together. Each broken link is printed as `BADANCHOR page:line -> target` with
the reason, and the last line counts the fragment links it checked.

Two more kinds of reference are checked in the default run. In `README.md` and `CLAUDE.md`, which
a reader opens before either book, an HTML `<img src="…">` must name a file that exists, since the
title page's picture is a raw tag no Markdown link check sees. And every `X-DocPath` a port under
`src/` ships, in a KCM's metadata or a desktop file, that names `kdos/<page>.html#<id>` (the
`help:` form a settings page uses) or `file:///usr/share/doc/kdos/<page>.html#<id>` is checked as a
link to `docs/kdos/<page>.md#<id>`: `kdos-doc` installs the book's pages under those names, so a
renamed heading is a help address that names nothing.

The package pages of *KDOS from Scratch* are generated and never committed, so a link to one is
checked against the tree instead: on a `docs/kfs` page, a bare `#port-<name>` must name a recipe
(`+` spelled `x`), `#phase-<name>` a directory under `script/phases`, and
`#shelf-<phase>-<shelf>` a phase and a shelf. The same anchor after a page name, or on a
`docs/kdos` page, is refused, because neither the renderer nor GitHub can resolve it.

## booknav.py

Every chapter of both books ends with a footer after a `<!-- book-nav -->` line: the part and
chapter number, then Previous, Contents and Next. `python3 testing/booknav.py` checks every footer
and exits 1 naming each page whose footer differs from the one it would write; `--write`
(`make booknav-write`) rewrites them. In `docs/kdos` the order and the titles are read from
`docs/kdos/README.md`'s contents, which stays the authority, and a chapter number out of sequence
fails the check. In `docs/kfs` the order is the rendered book's, and booknav writes the contents
too, between `<!-- book-contents -->` and `<!-- /book-contents -->` in `docs/kfs/README.md`. A
part's `<!-- kfs:order … -->` directive gives its page order, and a `[Label]` written between the
names heads the pages after it in the contents with a `#### Label` line; the rendered book ignores
the labels. In a part that uses labels, a page its directive does not name fails the check, because
it would otherwise be listed under the last label. Its
chapter numbers count the generated chapters (one per shelf of each package phase), which only a
render knows, so booknav runs `kfsgen.py` into a temporary directory and reads the result. A recipe
that gives a phase its first port on a new shelf therefore renumbers every later chapter, and the
check fails until `--write` is run.

## kfscheck.py

`python3 testing/kfscheck.py` (`make kfscheck`) holds *KDOS from Scratch* to the tree it is
rendered from. `--only` runs a subset by name:

| Check | Fails when |
|---|---|
| `sidecars` | A `kfs.md` beside a recipe has a section other than About, Flags, Patches or Notes; a `## Flags` bullet names a flag the port's `build.sh` does not name whole (a prefix of a longer flag or value does not count), or names none; a `## Patches` bullet names a file that is not in the port's directory |
| `notes` | A new or changed port meeting a Tier B criterion (a patch, vendored dependencies, a `build.sh` of more than 30 lines, 20 or more reverse dependencies) has no `kfs.md` with an About or a Notes section |
| `patches` | A new or changed `.patch` or `.diff` under `ports/` or `src/` has no `Subject:` line before its first `diff `, `--- ` or `Index: ` line |
| `snapshot` | Only with `--strict`: a port's version and release in `docs/kfs/data/packages.tsv` are not its recipe's. Without it the count is printed |
| `stamp` | `docs/kfs/data/recipes.stamp` is not this tree's digest, which covers every recipe (by its recipe hash) and every `kfs.md`, except a port that links a book under `docs/`; the phase lists and environments; the `00_cross` and `10_bootstrap` step scripts; `ports/shelves`, `ports/sources.idx`, `testing/optdeps.py` and `fs/etc/os-release`; and every included file; `make book-stamp` rewrites it with no built tree, `make book-data` after a build |
| `includes` | A `kfs:include` names a file that does not exist; or its `lines=A-B` lies outside the file, carries no `pin=`, or holds other lines than the ones its pin was taken from. When the pinned lines are elsewhere in the file the failure says where, and `kfscheck.py --repin` moves the range there and pins every unpinned one; a range whose lines changed is never repinned, so the prose around it is read again first |
| `kernel` | A row of the option tables in `docs/kfs/14-kernel/configuring.md` disagrees with the `linux` port's `kdos.config`. A row's first cell is the backquoted `CONFIG_…` symbol and its third the backquoted value: `y`, `m`, `n` for a `# CONFIG_… is not set` line, or the text after `=`, quotes kept. A row also fails when `kdos.config` does not set its symbol, since a kernel default is checked by nothing, and so does a repeated symbol, a row of fewer than four cells, or a page with no rows |
| `render` | `kfsgen.py` leaves a port unplaced or prints a warning |

New or changed means on no remote yet: the paths that HEAD's commits not held by any
`refs/remotes/*` ref add or modify, the working tree's changes against HEAD, and its untracked
files. A move that keeps the content whole is not a change, so reshelving a port does not make its
old patches new; a file already new or changed stays so when a later commit moves it, because the
commits are read oldest first and a move carries the state to its new path. `--base REF`, or `KDOS_CHECK_BASE=REF`, measures against `REF` instead. With no
remote-tracking ref and no base, or outside a checkout, the two gated checks print how many ports
and patches they would refuse and pass. `testing/preflight.sh` runs `--only patches`, and
`script/hooks/pre-push` applies the same `Subject:` rule to the commits it pushes and then runs
`docscheck.sh` and `kfscheck.py` over the working copy; `KDOS_SKIP_PATCH_CHECK=1` and
`KDOS_SKIP_BOOK_CHECK=1` skip those two parts of the hook.

## Measuring frame rate

Three different numbers are called "fps", and a measurement that does not say which one it took
cannot be compared with anything.

| Number | Counts | Decided by |
|---|---|---|
| Render rate | Frames the application finished drawing | The driver and the GPU, and whether the swap waits for a refresh |
| Compose rate | Frames KWin composited out of its clients | KWin, which paces off its output's refresh |
| Present rate | Frames that turned into light | The display mode, and the presentation path under it |

Under `make run-hw` the virtio-gpu virtual display runs at 60 Hz. A number above 60 there is a
render rate: those frames were drawn and thrown away, and nothing measured in the emulator can show
more than sixty frames a second reaching a screen. Raising a software cap raises the first number
and cannot raise the third. What a cap costs on a 144 Hz panel is a claim about real hardware and
has to be measured there.

KWin's own frame counter is its Show FPS effect, which draws the compose rate over the screen while
it is loaded:

```sh
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect showfps
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect showfps
```

It counts only frames KWin painted, so it says nothing while the screen is static; see
[Harness traps](#harness-traps).

### The overlay, which is already on every machine

mesa is built here with `-D gallium-extra-hud=true`, so a Gallium driver draws its own overlay over
any GL or GLES client, with no extra software and no change to the program:

```sh
GALLIUM_HUD=fps es2gears_wayland
GALLIUM_HUD=fps+frametime glmark2-es2-wayland     # both curves in one pane
GALLIUM_HUD=simple,fps es2gears_wayland           # text, no graph
GALLIUM_HUD=csv+fps+frametime es2gears_wayland    # values to stdout, for a script
GALLIUM_HUD=help es2gears_wayland                 # every name this driver can draw
```

In the syntax, `+` shares a pane, `,` opens a pane below, `;` opens a column, and `.w`/`.h`/`.x`/`.y`
size and place one. `GALLIUM_HUD_PERIOD` is the update interval in seconds; `0` means every frame.

It is a GL instrument. The frame sources are in every Gallium build; `gallium-extra-hud` adds the
disk, network and CPU-frequency ones. A Vulkan program draws no overlay: `vkgears` prints its own
rate instead, and `vkcube --c <n>` runs a fixed number of frames so an external clock can do the
arithmetic.

### Taking the cap off

A GL client that waits for a refresh is measuring the display, not the machine. `vblank_mode` is a
driconf option, and an environment variable of the same name overrides both the default and any
`drirc`, so it needs no cooperation from the program:

```sh
vblank_mode=0 es2gears_wayland          # never synchronise, ignoring the application's choice
MESA_VK_WSI_PRESENT_MODE=immediate vkgears
```

`glmark2` asks for swap interval 0 itself unless `--swap-mode fifo` is given, so its score is a
render rate by construction and is comparable between machines rather than between panels.

### Which tool answers which question

| Question | Tool |
|---|---|
| How fast can this machine draw a trivial scene | `es2gears_wayland`, which prints `N frames in X seconds` every five seconds |
| How fast can it draw real ones, as one comparable score | `glmark2-es2-wayland`, or `glmark2-wayland` for desktop GL |
| The same with no compositor in the way | `glmark2-es2-drm` / `glmark2-drm` from a text console while no compositor holds the display; `glmark2-es2-gbm` / `glmark2-gbm` render offscreen and need no display |
| The same for Vulkan | `vkgears`, or `vkcube` for a swapchain whose present mode can be chosen |
| Does this machine have a Vulkan driver, and which | `vulkaninfo --summary`. Run it first under an emulator, because a Vulkan tool falls back to lavapipe on the CPU without saying so |
| Which EGL renderer, extensions and configs a client gets | `eglinfo` |
| What an X11 client sees through Xwayland | `glxinfo`, `glxgears` and `es2gears_x11`, which exist only in a box |

`es2gears_wayland`, `eglinfo` and `vkgears` come from the `mesa-demos` port, the six `glmark2`
flavours from `glmark2`, and `vulkaninfo` and `vkcube` from `vulkan-tools`.

### Desktop GL and GLX

A Wayland client reaches desktop GL through EGL, never GLX. `glmark2-wayland` binds
`EGL_OPENGL_API` and then opens the GL entry-point library by name, `libGL.so` first and
`libGL.so.1` second, and prints `Error loading GL library` if neither answers.

`libglvnd` is built with `x11` and `glx` enabled, so it installs `libGL`, `libGLX`, `libOpenGL`,
`libEGL` and `libGLES` with their `gl.pc` and `glx.pc`. `libGLX` dispatches to mesa's
`libGLX_mesa`. mesa is built `-D glx=dri -D platforms=wayland,x11`, which is what an X11 client
under Xwayland draws with: `libGLX_mesa` behind glvnd's `libGL` and `libGLX`, and the Vulkan X11
WSI. Xwayland is built `-Dglx=true` (with `-Dglamor=true` and `-Ddri3=true`), so an X11 client's
`glXChooseVisual` finds the GLX extension. libepoxy is built `-Dglx=yes` and resolves `glX*`
through `libGL.so.1` at run time. A Wayland client never loads `libGLX_mesa`, but `libEGL_mesa`
and the Vulkan drivers link `libxcb` and `libX11` for their X11 platform, so the X client libraries
are on the host either way.

To confirm what an image carries, after a build's `70_image` phase has written `build/iso_root`:

```sh
unsquashfs -ll build/iso_root/system.sfs | grep -E 'libGL|libOpenGL|libGLX'
ls build/fs/usr/lib/libGL.so*                # the same question of the build tree
```

The `mesa-demos` port is built for Wayland alone (`-Dx11=disabled`), so `glxgears`, `glxinfo` and
the X11 EGL demos are not on the host. They come from a box: the `app.mesa-utils` catalogue row
builds Debian's `mesa-utils` on the base image and puts `glxgears`, `glxinfo` and `es2gears_x11` on
the host's path as shims. Those three names are the ones the host does not carry, so a shim never
shadows a host binary; any other program in the box is reached with
`kdos-appbox -b app.mesa-utils run`:

```sh
kdos app install app.mesa-utils
es2gears_x11                                  # through EGL and Xwayland
glxinfo                                       # the GLX extension Xwayland serves
glxgears                                      # through GLX and Xwayland
kdos-appbox -b app.mesa-utils run eglinfo     # the boxed copy, not the host's
```

That box is also the host-versus-box measurement: its `es2gears_x11` runs the same test as the
host's `es2gears_wayland`, through Xwayland and a container, and the difference between the two
numbers is what that path costs.

### Where the ceilings are

KWin has no frame-rate constant of its own. It paces off its output's refresh and so follows
whatever mode is set; a number that stops at a round figure is the mode, the driver or the client,
never a ceiling written into KDOS.

## The other harnesses

"In the guest" means the script runs as root inside a booted KDOS, sent there with
`vnc-shot.py --root-script`.

Two of these need application packs that the shipped ISO does not carry. The medium holds the
catalogue but no application packs (see
[Where a pack comes from](../03-architecture/packs-and-boxes.md#where-a-pack-comes-from)), so on a
stock ISO:

- `appsweep.sh` reads the pack index at `/mnt/iso/packs/PACKAGES`, finds fewer than ten packs, and
  refuses to sweep with exit status 2;
- `packlane.sh` looks for packs on the medium at `/mnt/iso/packs`, or in the store at
  `/var/lib/kdos/packs` when there is no medium, and reports every check that needs one (a base,
  `rt-gtk`, the index, `app.vorta` (`PACKLANE_APP`) and `app.meld` (`PACKLANE_GTKAPP`)) as
  failed.

Run them against a medium or a store you have put those packs into. `install-to-disk.sh` chooses
no applications unless `INSTALL_APPS` names catalogue group ids, and a group it names is recorded
in the target's `/var/lib/kdos/apps-pending` for the first login, not built.

| Harness | Runs | Does |
|---|---|---|
| `packlane.sh` | In the guest, session up | The pack lane end to end: the daemon, the keyring, an install from the medium or the store, the launchers, a box on a pack base, the energy attribution and a timed launch, reporting pass, fail or skip with a reason and carrying on past a failure. It exercises no rollback, and its checks for a base and `rt-gtk` in the store fail until packs have been imported |
| `install-to-disk.sh` | In the guest, with `--disk` | Runs the installer unattended into the attached disk, with its interface on `/dev/tty3`. Every 15 seconds it prints a heartbeat: the seconds elapsed, the space used on the target mounted at `/mnt/kinstall`, and the last line of `/var/log/kinstall.log`, so that a slow copy can be told from a wedged one. Before it starts it reads the payload from `kinstall --dump probe` and the disk's size with `blockdev`, and refuses with exit status 2, printing both in MB, when the disk is smaller than the payload, the 512 MB ESP and 2 GiB; `kinstall`'s Prepare step then refuses, before anything is written, a disk smaller than the payload plus 768 MiB. The copy writes about the whole payload (47661 MB on the current tree), so a qcow2 target grows on the host by that much, and `vnc-shot.py` refuses an install run whose qcow2 could outgrow the host's free space. It gives up after `INSTALL_CAP` seconds (default 5400) and prints the log's last 40 lines at the end. The answer file sets root's password to `kdos`. `INSTALL_DISK` names the target (default `/dev/vda`); `INSTALL_LUKS=1` makes the same run with an encrypted root, with the passphrase in `INSTALL_LUKS_PASS` (default `kdosdisk`). It then opens what it wrote: the ESP with its `limine.conf` and the A/B boot state, the volumes of the LVM group `kdos`, the fstab with the pack store's bind line, the `kdos` account, SDDM's autologin file, and the shared pack store under `/home/.kdos/packs`. The second half boots the disk with `--no-cdrom`, and the rig logs root in on the serial console with `--root-password` |
| `bios-boot.sh` | On the host | Boots the ISO as a USB stick on an xHCI controller under SeaBIOS, with no OVMF anywhere. `vnc-shot.py` always boots UEFI, so it cannot answer this. It waits `KDOS_BIOS_SECS` seconds (default 90), saves the screen as `build/shots/bios-screen.ppm` and the serial console as `build/shots/bios-serial.log`, then reads the log for `Found KDOS media` and `Switching to new root`. The stick is read-only, so the run never writes the image and can run while rig VMs have it attached: it prints `bios-boot: ok` and exits 0 when both are there, and `bios-boot: FAILED` with exit status 1 when either is missing. QEMU itself is killed rather than shut down, so its own exit status means nothing |
| `appsweep.sh` | In the guest, `--boot-disk` | For each catalogue application: install it off the medium, launch it through its shim (a terminal program in Konsole, as Kickoff launches it), wait for KWin's window list (`kdos hey list`) to name its window and its box, photograph it, and remove the box. An X11 window has no box, since its Wayland client is Xwayland, so it is matched by its class against the shim's name and the row says so. Usage `appsweep.sh <outdev> <runtime\|all> [max]` |
| `appreport.sh` | On the host, needs ImageMagick | Reads what the sweep wrote to the scratch disk and renders it as a table and a contact sheet, flagging windows that mapped but painted nothing. Usage `appreport.sh <scratch.img> [outdir]` |
| `appbox-smoke.sh` | In the session, as the desktop user (`su - kdos -c 'WAYLAND_DISPLAY=wayland-1 testing/appbox-smoke.sh'`) | Does every boxed application's launcher start something: a cheap `command -v` pass (`--probe`), then real launches; names on the command line limit it to those |
| `hostcheck.sh` | In the guest | Whether the shipped binary a caller resolves by name is the one that can do the job, for example which `blkid` answers first on the search path, asked against a real LUKS container |
| `oomd-fire.sh` | In the guest | Puts the machine under real memory pressure and reads `kdos-oomd`'s own status for the kill count and victim, then repeats it after a stall with memory still plentiful and checks the kernel's own OOM count did not rise. Victim *selection* is tested offline by `kdos-oomd --fixture`; this proves the daemon wakes |
| `aboutcheck.sh` | In the guest, as any user; or `--root DIR` in a chroot | Every program the About page runs prints two lines, fast, in every condition a real machine meets; see [The desktop's defaults](#the-desktops-defaults) |
| `usability.sh` | On the host | Drives the desktop with the pointer and chords only and leaves a numbered contact sheet under `build/shots/usability` (`KDOS_USABILITY_OUT`), to be read against `testing/usability.md`. It waits for the readiness gate, closes Welcome Center, and aims at the KDOS desk's bottom panel, whose centre line it takes as twenty pixels above the screen's bottom edge; a run at another font size, scale or desk has to take the positions again from its own desktop shot |
| `bootcheck/` | On the host | Plumbing for scripted boots with no display: `boot.sh [soft\|gl]` starts QEMU detached, `guest.py` runs a command on the serial console, `type.py` types into the focused window, `sock.py` owns one QEMU socket. `bootcheck/README.md` describes them |
| `quick.sh`, `quickpatch.sh` | Host, then guest | [The fast loop](#the-fast-loop) |
| `plasma-probe.sh`, `session-cost.sh` | Guest; host, then guest | [Checking the session](#checking-the-session), [Time to the panel, and memory at idle](#time-to-the-panel-and-memory-at-idle) |
| `atspi/` | Host, then guest; or the development image | [Goldens](#goldens) |
| `qemu-audio.sh` | Sourced by the run targets and the rig | Picks a working audio backend rather than hard-coding one, because QEMU aborts at start-up on a backend its build lacks, and sets the mixer's rate, because QEMU's default is 44100 and the guest drives the codec at 48000 |
| `qemu-hw/` | On the host, needs Docker and the NVIDIA container toolkit | The containerised QEMU with accelerated (virgl) graphics behind `make run-hw` and `make rundisk-hw` (`run.sh iso` and `run.sh disk`, built from its `Dockerfile`). Beside them, `probe.py` boots the ISO with blob resources and prints what the guest's kernel reports about the virtio GPU (its command line, the `virtio_gpu` and DRM messages, the blob and host-visible features), to tell a blob-capable QEMU from plain virgl. |
| `rig-image.sh`, `devdeps-image.sh` | On the host | Build the rig image (`kdos-qemu-py`, from `Dockerfile.qemu`) and the development image (`kdos-devdeps`, from `Dockerfile.devdeps`) |

`testing/notes/` holds the measurements the pack format rests on (`packs-w0.txt`: trailing bytes
after an EROFS image, forced ownership, and the other assumptions). It is read, not run.

`prepare_base.py`, `mini_build.py`, `test_runner.py` and `report_gen.py` build ports one at a time,
each in a fresh container, outside `make build`. They answer whether a single recipe builds against
a base system on its own, without the state an incremental build tree has accumulated; they do not
replace the build. They need Docker, and `prepare_base.py` also runs `sudo`:

- `prepare_base.py` builds the `os-dev` build container, runs phases `00_cross` to `31_compilers`
  into `build_test/` through `mini_build.py` inside it, reading each package phase's
  `packages.txt` or `packages.d/`, packs the result as `testing/kdos_fs.tar.gz`, writes
  `testing/Dockerfile.base` and builds the image `kdos-base-test` from it;
- `test_runner.py` runs `kpkg install -f <port>` for each port under `ports/core`, found one shelf
  down and named by its bare name (or the one named by `--package`), in a fresh `kdos-base-test`
  container, recording results in `testing/test_results.json` and logs under `testing/logs/`;
- `report_gen.py` summarises those results as `testing/report.html`.

A KDE module is not test-built that way: its Frameworks, Plasma and Qt exist only in this tree, so
it is test-built in the tree's own chroot with `CC`, `CXX`, `CFLAGS` and `LDFLAGS` exported from
`script/env/chroot.env`, or with `make build BUILD_ARGS="--phases <phase> --rebuild <port>"`. A
chroot test without those flags passes where the real build fails.

### Undeclared link dependencies

`testing/depdrift.py` reads a built tree and names every package that links against, or whose
`.pc` file requires, a package outside its declared `depends` closure. Run it from the repository
root after a build, in a container with Python (`kdos-devdeps` or `os-dev`), and before trusting a
phase built with `kdosbuild --port-jobs`:

```sh
python3 testing/depdrift.py --root build/fs        # --repo <repo> overrides each phase's PORT_REPO; --strict exits 1 on a DRIFT line
```

It maps each path to its owner from the manifests in `<root>/var/lib/kpkg/db`, reads the
`DT_NEEDED` sonames of every ELF64 file a package owns and the `Requires` and `Requires.private`
modules of every `.pc` file, and resolves them under `usr/lib`, `lib` and `usr/local/lib` and the
`pkgconfig` directories. The declared closure and each port's phase and serial position come from
the recipes and phase lists, resolved as `phaseclosure.py` resolves them. Each undeclared owner is
classified:

| Class | Printed as | Means |
|---|---|---|
| Same phase | `DRIFT` | The owner is installed by the package's own phase outside its order run. Only the serial order made it present; under `--port-jobs` it can be absent. Add it to `depends` |
| Order run | `note` | The owner is installed by the same phase from its order run |
| Earlier phase | `note` | The owner is installed by an earlier phase, so it is present either way |
| Unplaced | `note` | The package or the owner is in no phase list |

It is advisory and preflight does not run it, because it needs a built tree. Exit status is 0
unless `--strict` is given and a `DRIFT` line was printed. A dependency loaded with `dlopen`, run
as a program or included only as a header is invisible to it.

### Recipe metadata and switched-off features

Two scripts read the recipes with no build tree, and preflight runs both:

```sh
python3 testing/recipemeta.py            # every finding; --brief prints a count per rule
python3 testing/optdeps.py               # every switch that turns off a feature a port provides
python3 testing/optdeps.py --count       # the number of those switches only
python3 testing/optdeps.py --claims      # every kfs.md line saying something is not a port
```

`recipemeta.py` reads each `kpkgbuild` the way `kpkg info` and the version checker do, the first
line of a key taken literally, and reports a recipe with no `description`, `homepage` or
`license`, a `license` that is not an SPDX expression, a first source that spells the version
instead of `$version`, and a `description` that is not exactly its port's row in the
[ports catalogue](../06-reference/ports-catalogue.md), with ports that have no row and rows that
name no port. It exits 1 on any finding, and preflight fails with it.

`optdeps.py` reads every `build.sh` under `ports/core` and `src/` for `--without-X`,
`--disable-X`, a CMake `-D…=OFF` (or `false` or `no`), `-DCMAKE_DISABLE_FIND_PACKAGE_X=ON`, and a
meson `-DX=disabled` or `-DX=false`, and maps X to a port by its name, by
`lib<X>`, by `python3-<X>` and through an alias table of upstream spellings (`numa` is `numactl`,
`PyGObject` is `python3-gobject`, `libstemmer` is `snowball`). A switch whose X is a port is either
a choice or a feature lost to the belief that the library is missing, and the switch cannot say
which, so the script is a report: preflight prints its count and never fails on it. An upstream
spelling that maps to no port is silent, so a new port whose upstream name differs from its own
needs its row in the alias table. `--claims` reads every line of each port's `kfs.md`, where the
reasons for its flags live, and the comment lines of its `build.sh`, and prints each line that says
something is not a port, naming its file (`<dir>/kfs.md:N` or `<dir>/build.sh:N`) and marking with
a `*` each name in it that is one; on this tree it prints 287 lines.
The features whose library really has no port are listed in
[known gaps](../06-reference/known-gaps.md#features-whose-library-has-no-port).

### Debug information in the built tree

`testing/debuginfo.py` is the gate on shipped DWARF. `kpkg` strips no debug information, so a file carries debug
sections exactly when its port's flags produced them, and no port is meant to. Run it on a built
root after changing a recipe's compile or link flags, or after adding a port whose build system
defaults to `-g`:

```sh
make debuginfo                                    # build/fs, from a container or as root
python3 testing/debuginfo.py --root <root>        # --threshold BYTES, --files lists every file
```

One walk over `usr/` reads each regular file's first four bytes, and each ELF file's section table
is parsed in place; nothing is spawned per file, and `build/fs` takes about three minutes. Every
`.debug_*` and `.zdebug*` section that occupies bytes is summed. Kernel modules, firmware, the
cross-target sysroots (`arm-none-eabi`, `avr`, `riscv*-elf`) and BPF objects are left out. The
totals are grouped by the package whose manifest in `<root>/var/lib/kpkg/db` names the file, largest
first; a manifest path is keyed as `kpkg` keys it, with a top-level `/bin`, `/sbin`, `/lib`,
`/lib64` or `/lib32` alias folded into `/usr`, so a file a package installed through the alias is
not misread as unowned. A file no manifest names is listed under `(unowned)`.

One pattern is allowed: ghc's `libHSrts*_debug-ghc*.so`, the runtimes `ghc -debug` links. Carrying
DWARF is what they are for, so they are counted on a line of their own and never in the total.

Exit status is 1 when the DWARF outside the allowlist exceeds the threshold, 1 MiB unless
`--threshold` says otherwise, and 0 when it does not. It changes nothing, and preflight does not run
it, because it needs a built tree.

This check and the three after it (`unowned.py`, `linkcheck.py` and `entrycheck.py`) read the root
through one shared module, `testing/treescan.py`, which is not run on its own. It walks the root
once, recognises an ELF file by its first four bytes, parses its section, dynamic and interpreter
tables in place, reads the `kpkg` manifests with `tar`'s quoting undone, and folds the merged-`/usr`
aliases into their targets. How a path is keyed is therefore decided in one place for all four.

### Files nothing owns

`testing/unowned.py` lists the files of a built root that no package manifest and no `fs/` path
provides. Such a file is one no removal takes away and no update replaces: a build that wrote
outside its package root, a tool run against the live root, or a manifest that lost the line.

```sh
python3 testing/unowned.py --root build/fs        # --fs <repo>/fs; --all lists every path
python3 testing/unowned.py --root build/fs --fail-under usr/share/doc   # fail only on findings there
```

Paths are keyed as `kpkg` keys them, and a manifest line is read with `tar`'s quoting undone, so a
name outside ASCII matches the file on disk. What something other than a package writes on purpose
is subtracted: the mount points and build scratch (`proc`, `sys`, `dev`, `run`, `tmp`, `var/tmp`,
the chroot's `/kdos` and `/ports`, `kpkg`'s caches), the homes and `/etc/skel`, `kpkg`'s database,
the merged-`/usr` links and the `var/run` and `var/lock` links the bootstrap makes, `kpkg` with
its four names and `/etc/kpkg.conf` and `kinstall`, which `10_bootstrap` compiles into the root and
no port packages, the `passwd-`, `group-`, `shadow-` and `gshadow-` copies shadow's tools keep
beside the account files they edit, the files
generated over every package (the shared indexes `kpkg`'s triggers and `70_image` rebuild, depmod's
module indexes, `/etc/ssl/cert.pem`, the launcher table, the `SOURCES` offer and the initramfs
with its module list in `/boot`), and every `__pycache__`: bytecode a package ships is owned, and
what the build's own tools wrote is unowned until `020_cleanup.sh` deletes it, which a run on an
unfinished tree would otherwise list.

The report groups paths by directory, largest group first. Exit status is 1 when it reports a path
and 0 otherwise. `--fail-under DIR`, which may be repeated, keeps the report whole but makes the
status 1 only for a path under one of the named directories, and prints each such path on a
`FAIL unowned` line. `70_image` runs it in `087_gates.sh` with `--fail-under` for
`usr/share/locale`, `usr/share/man`, `usr/share/doc` and `usr/share/info`, after `020_cleanup.sh`
has removed every unowned file there that the
[size and language policy](writing-ports.md#size-and-language-policy) prunes: what is left there
unowned is documentation that would ship and nothing could remove, and it stops the image. Any
other finding is a report the step does not stop on.

### Run-time references

`testing/linkcheck.py` fails a built tree whose programs reach for something the package does not
declare or the root does not hold. Run it from the repository root, which it reads for the recipes
and phase lists:

```sh
python3 testing/linkcheck.py --root build/fs      # --all also lists the unresolved warnings
```

Every file a package's manifest names is read once:

| Line | Means |
|---|---|
| `FAIL undeclared` | An ELF file's `DT_NEEDED` resolves, as musl's loader resolves it (the object's `RPATH`/`RUNPATH` with `$ORIGIN`, then `etc/ld-musl-x86_64.path`), to a file owned by a package outside the consumer's transitive `depends`. musl, gcc and linux-headers are exempt. Add the owner to `depends` |
| `FAIL interpreter` | A script's `#!` interpreter or an ELF file's `PT_INTERP` under `/usr` (or `/bin`, `/sbin`, `/lib`) does not resolve to an executable, or `#!/usr/bin/env NAME` names a command on no `PATH` directory of the root |
| `FAIL openssl` | One process would map both `libcrypto.so.3` and `libcrypto.so.4`, which musl binds to whichever came first. The sets read are each object's own `NEEDED` closure, each Qt 5 plugin together with `libQt5Network.so.5`, and each Python extension module together with `_hashlib` and `_ssl`, except those of `python3-pyqt5`, whose pairing prints `WARN openssl` instead because it is [a known gap](../06-reference/known-gaps.md#a-pyqt5-program-that-also-uses-pythons-ssl-or-hashlib-can-crash) |
| `WARN python` | A `#!` names bare `python`. It runs whatever `python` the `PATH` finds; the script should name `python3` |
| `WARN unowned` | A `DT_NEEDED` resolves to a file no package owns |
| `WARN unresolved` | A `DT_NEEDED` the object's own search path does not find. A plugin can still load when its host has mapped the library, so it is counted, and listed with `--all` |

The declared closure is resolved per package over its phase's `PORT_REPO`, as `depdrift.py`
resolves it. Unlike `depdrift.py`, every undeclared owner fails, wherever it is installed: a
dependency that only the build order supplied is one a removal or an update can take away. A
library loaded only with `dlopen` is invisible to it. Exit status is 1 when a `FAIL` line was
printed. `70_image` runs it in `087_gates.sh`, and a failure stops the image.

### Links, launchers and rules

`testing/entrycheck.py` fails a built tree that carries a reference to nothing:

```sh
python3 testing/entrycheck.py --root build/fs
```

| Line | Means |
|---|---|
| `link` | A symlink that resolves to nothing inside the root. A target under `/proc`, `/sys`, `/dev`, `/run` or `/tmp` is a run-time path and is not checked |
| `exec` | A desktop entry's `Exec=` (applications, autostart and the skel launchers) or a D-Bus service's `Exec=` whose program is not an executable in the root, by path or on the `PATH`. An entry whose `TryExec=` does not resolve is hidden by specification and is skipped |
| `icon` | A visible desktop entry (no `NoDisplay=true`, no `Hidden=true`) whose `Icon=` names no file, and no icon in any theme under `usr/share/icons` or in `usr/share/pixmaps` |
| `udev` | A `RUN`, `PROGRAM` or `IMPORT{program}` key in a udev rule whose program does not exist, by path or in `usr/lib/udev`. udev reports that at debug level only |

Every path is resolved against the root, never the host. Exit status is 1 when anything is
reported. `70_image` runs it in `087_gates.sh`, and a finding stops the image.

## What is not tested

This section lists what the harnesses above do not cover, so that a green run is not read as
evidence for it.

Nothing here tests the build itself ([How KDOS is built](how-kdos-is-built.md)), which takes hours
and a container. The self-test runs the orchestrator against a synthetic tree, which proves its
logic, not that the distribution builds.

The memory daemon fires only under `testing/oomd-fire.sh`, a rig run rather than a self-test block,
because it needs a booted machine with real memory to exhaust.

The pack lane (the [import lane](../06-reference/glossary.md#lane), exercised by `packlane.sh` and
`appsweep.sh`) cannot run against the shipped ISO alone, because the packaging step bakes no
application packs into it. Nothing tests the lane until someone provides packs, and nothing tests a
pack rollback on a booted machine; see [The other harnesses](#the-other-harnesses).

The login screen, the lock screen, logout, suspend and resume and VT switching are driven by the
[login flows](#the-login-flows). Autologin, the lock screen, suspend and resume, VT switching and
the tty-leak check have run on the rig against the live ISO, and all but the screen after resume
pass; the greeter and logout flows need an installed disk and have not run. User switching is driven by no script; the rig can reach it and a person reads the
photographs.

KWin's effects and scripts, the KDOS ones included, are seen working only on the rig. In the
development image they are installed and linted, never run, because KWin needs a render device a
container does not have; a photograph of Phosphor mode needs `--gl`.

Nothing that runs unattended has hovered a button in Plasma. `plasma-probe.sh` proves the panels
are there and the AT-SPI goldens prove what each KDOS page says; neither proves a tooltip, a hover
state or a drag looks right. That is a person's job with the rig.

A golden cannot see a colour. The accessibility tree carries roles, names and states, not pixels, so
a page drawn in the wrong colours dumps identically to a right one, and the eight accents are
checked by the contrast floors on the generated schemes, not on screen.

The pointer's own pixels are in no test. KWin draws the cursor, and a photograph of a moving pointer
is the only place to look at it.

## See also

- [Developing](developing.md): the build and iteration loops these fit into
- [How KDOS is built](how-kdos-is-built.md): the full build these harnesses let you avoid
- [Writing desktop software](writing-desktop-software.md): KDOS's Plasma code from the author's side
- [The C libraries](c-libraries.md): what the assertions cover
- [Build troubleshooting](build-troubleshooting.md): when preflight is not enough
- [Writing ports](writing-ports.md): recipes, sources and the pre-push hook
- [The ports catalogue](../06-reference/ports-catalogue.md): every port `test_runner.py` can build one at a time
- [Glossary](../06-reference/glossary.md): the terms this chapter uses
- [Known gaps](../06-reference/known-gaps.md): the full list of what does not exist

<!-- book-nav -->
---

*Part V — Building and developing, chapter 37.* Previous: [36. Writing desktop software](writing-desktop-software.md) · [Contents](../README.md) · Next: [38. The ports catalogue](../06-reference/ports-catalogue.md)
