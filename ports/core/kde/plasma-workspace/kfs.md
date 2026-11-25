## About

Plasma Workspace is most of what a Plasma session is: `plasmashell`, which draws the desktop, the
panels and their applets; `startplasma-wayland`, `plasma_session` and `ksmserver`, which start the
session and end it; KRunner; the notification server; Klipper; the logout screen; KSplash;
`xembedsniproxy` for X11 tray icons; and the System Settings modules for colours, fonts, cursors,
icons, the global theme, notifications, Region & Language, users, autostart and default
applications. Its libraries (`libkworkspace`, `libtaskmanager`, `libnotificationmanager`,
`libklookandfeel`) are what [plasma-desktop](#port-plasma-desktop), [powerdevil](#port-powerdevil)
and the other session modules link. It runs under [kwin](#port-kwin) and needs
[kscreenlocker](#port-kscreenlocker), whose D-Bus interface definitions the configure reads.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compile none of the interface translation catalogues in `po/`.
  kpkg removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D WITH_X11=ON` — Build the X11 parts that serve Xwayland clients: `xembedsniproxy`, which puts
  an X11 program's tray icon in the system tray, `gmenu-dbusmenu-proxy`, which hands an X11 GTK
  program's menu bar to the global menu applet, Klipper's X11 clipboard and the Fonts settings
  module. `ksmserver`, the session manager every Plasma session runs, is built only with this
  switch, so turning it off leaves no session to start. It links the X client libraries in
  `depends` and starts no X server.
- `-D WITH_X11_SESSION=OFF` — Install no X11 session. The 6.8.2 build declares the option and reads
  it nowhere, so it changes nothing in this release; it is passed so that a release that reads it
  again cannot put a `startplasma-x11` and an `xsessions` entry on the system, which has no X
  server.
- `-D INSTALL_SDDM_WAYLAND_SESSION=ON` — Install `/usr/lib/sddm/sddm.conf.d/plasma-wayland.conf`,
  which runs [sddm](#port-sddm)'s greeter on `kwin_wayland` instead of an X server. Upstream leaves
  it off and calls the Wayland greeter a beta; a system without an X server has no other greeter.
- `-D GLIBC_LOCALE_GEN=OFF` — Build no `localegenhelper`, the polkit helper the Region & Language
  module runs to add a locale to `/etc/locale.gen` and run `locale-gen`. musl has neither; its
  locales are the files [musl-locales](#port-musl-locales) installs. Without the switch the module
  offers an action that fails, and the configure requires polkit-qt6 for it.
- `-D GLIBC_LOCALE=OFF` — Offer every locale Qt's CLDR data carries in the Region & Language
  module's format lists. On, `localelistmodel.cpp` keeps only the locales with a definition file
  in the module's locale directory: the seventeen musl-locales installs, with no Japanese, Chinese,
  Korean, Arabic, Hebrew, Polish, Turkish or Greek. That directory holds no `C` file either, so the
  filter drops the C locale, and the row the model labels "ISO C Standard" by position becomes
  `cs_CZ`. A KDE program formats from Qt's CLDR data and the `LC_*` names in `plasma-localerc`,
  and musl accepts any locale name, so every listed region's formats apply. Off, the Language page
  writes the selected language as a `<locale>.UTF-8` name instead of matching it to a definition
  file, and the module hands AccountsService no definition-file name for the languages.
- `-D BUILD_GEOTIMEZONED=OFF` — Build no `geotimezoned`, the kded module that, once started from
  Background Services, sends the machine's public address to `geoip.kde.org` whenever the primary
  network connection changes and sets the system time zone to the answer through
  `org.freedesktop.timedate1`. The option is on by default on Linux, and the module is the only
  part of the package that asks a web service for anything on its own (see
  [Offline by construction](../../../../docs/kdos/01-philosophy/principles.md#offline-by-construction)).
  Off, the configure does not require NetworkManagerQt.
- `-D BUILD_CAMERAINDICATOR=ON` — Build the camera indicator applet, which shows in the system tray
  while an application is reading a camera. Upstream builds it only when a quiet `pkg-config`
  search finds libpipewire, which only the applet's tests link; the applet itself is QML over the
  `org.kde.pipewire.monitor` module from [kpipewire](#port-kpipewire), so the switch builds it
  whatever the build root holds.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_AccountsService=ON` — The Users settings module edits accounts
  through `accounts-daemon` from [accountsservice](#port-accountsservice) and reads its
  `accountsservice.pc`. Upstream treats it as a run-time package and only reports its absence.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_AppStreamQt=ON` — AppStreamQt, from [appstream](#port-appstream),
  is what lets KRunner offer software that is not installed and open it in
  [discover](#port-discover), and what gives Kickoff's entries their "Uninstall or Manage Add-Ons"
  action. Upstream builds both without it and says nothing. `appstream` must be built with its Qt
  library, or the configure stops here.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Fontconfig=ON` — With `-D CMAKE_REQUIRE_FIND_PACKAGE_Freetype=ON`:
  [fontconfig](#port-fontconfig) and [freetype2](#port-freetype2) are optional upstream, and
  without them the font installer and the Fonts settings module are not built.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KExiv2Qt6=ON` — The image wallpaper's chooser shows a picture's
  title and author, read from its Exif tags through [libkexiv2](#port-libkexiv2), which upstream
  uses only when it is found.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Baloo=ON` — The file search runner, KRunner's search of indexed
  files, is built only when [baloo](#port-baloo) is found.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbooks, through
  [kdoctools](#port-kdoctools), which upstream looks for as an optional component.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON` — No KUserFeedback, which would add the
  telemetry settings module and plasmashell's usage reporting. It is not a port; refusing it means
  a stray copy in the build root cannot be picked up.
- `-D CMAKE_DISABLE_FIND_PACKAGE_PackageKitQt6=ON` — No PackageKit integration, which Region &
  Language uses to install language packages and the session uses for offline updates. PackageKit
  is not a port, and KDOS installs software through its own package manager.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Patches

`locales.patch` points the Region & Language module's locale search at
`/usr/share/i18n/locales/musl`, where [musl-locales](#port-musl-locales) installs its definition
files. The module's `GlibcLocaleConstructor` scans that directory every time the module starts,
whatever `GLIBC_LOCALE` says. Upstream reads `/usr/share/i18n/locales` and has no option for the
directory; there the scan finds the `musl/` subdirectory and no file, and the module shows a red
error that it could not find the system's locales. Chimera Linux carries the same one-line change
(cports `main/plasma-workspace/patches/locales.patch`).

`region-no-generation.patch` makes an Apply in Region & Language end on the blue "Changes will
take effect the next time you log in" message alone. With `GLIBC_LOCALE_GEN=OFF` the module's
generator is `LocaleGeneratorBase`, whose only answer to a save is `userHasToGenerateManually`
with a fixed text telling the user to install fonts and generate locales from the distribution's
manual. Upstream's `kcmregionandlang.cpp` counts that signal as success for this generator, but
also shows the text as a red error, beside a "don't turn off the computer" warning, raised before
the call, that nothing takes down. The patch connects the signal, for `LocaleGeneratorBase`, to
`saveToConfigFile` and `takeEffectNextTime` only, and `save()` raises the warning only for a
generator that generates. musl needs no generation step: it takes any locale name. It is to be
offered upstream.

`kdos-bootctl-entry.patch` is KDOS's own. The program it runs instead of `bootctl` is
`kdos-bootctl` from [kdos-tools](#port-kdos-tools), whose `list --json=short` prints the slots in
the JSON shape the logout screen parses; see
[Counting and confirming](../../../../docs/kdos/03-architecture/boot-and-init.md#counting-and-confirming).

`sandbox-badge.patch` adds the task roles `SandboxEngine` and `SandboxName`, at the end of
`AbstractTasksModel::AdditionalRoles` so no existing role value moves: `org.flatpak` and the
application id for a task whose desktop entry has `X-Flatpak`, `io.kdos.appbox` and the box for one
with `X-KDOS-Box`, which every launcher `kdos-appbox genlaunchers` writes carries. The Task
Manager's tooltip reads them ([plasma-desktop](#port-plasma-desktop)'s patch of the same name).
They say where an entry launches, not what a running window is; the title-bar glyph, read from the
compositor's security context, is the authority on that. Proposed upstream with the KWin,
KDecoration and Breeze halves.

`notifications-group-expander.patch` makes a notification group in the history hide its oldest
entries and always offer the "Show %1 More" and "Show Fewer" button for them, with the count it
hides. Every KDOS Tools notification is in one group: `kdos`, `kdos-notice` and `kdos-runner` send
`x-kde-appname` and no `desktop-entry`. It changes four files.

The history model in `applets/notifications/main.qml` sets `expandUnread: false`. Upstream sets
it true, and `NotificationGroupCollapsingProxyModel::filterAcceptsRow` then keeps every unread
child newer than `lastRead` as well as the newest `groupLimit` (two), so a collapsed group hides
an entry from its middle, and which one depends on what has been read. Off, a collapsed group shows
its newest two and hides exactly the older ones.

The button's count is `GroupChildrenCountRole` less `ExpandedGroupChildrenCountRole`, both read on
the group's last visible row, and the delegate reads each again only when the model announces it.
Both are re-announced to every child row of a group when a row joins or leaves it.
`libnotificationmanager/notificationgroupingproxymodel.cpp` announces `GroupChildrenCountRole`, the
group's size, on all of a group's children when `tryToGroup` adds one, and on all remaining
children, by the group's own child count, when one is removed; upstream announces it on none when
a row joins, so the rows already shown keep the old size.
`libnotificationmanager/notificationgroupcollapsingproxymodel.cpp` re-announces
`ExpandedGroupChildrenCountRole`, the proxy's row count under the group, and `IsGroupExpandedRole`
after a source row is inserted or removed and after a `ReadRole` change. Without either, a group of
four shows three rows and no button, or a group of eight offers "Show 4 More" when five are hidden,
and an entry is never reachable.

`applets/notifications/FullRepresentation.qml` draws the button on the row followed by a top-level
row or by none. Upstream draws it on the row whose `ListView.nextSection` differs from its section,
the sender's desktop entry. A sender that names no desktop entry has an empty section, and
`nextSection` is empty past the list's last row, so a group of such a sender at the bottom of the
history, or followed by another one, draws no button. The next row is read from the model, which
QML does not track, so the list keeps a revision that the model's layout change, move, insertion,
removal and reset signals bump, and the binding reads it; without it a group re-sorted in place
would leave the button on a row that is no longer its last.

It is to be offered upstream.

`appstream-runner-name-match.patch` makes the Software Center runner in
`runners/appstream/appstreamrunner.cpp`, which gives KRunner and Kickoff their "Get …" results,
offer an application that is not installed only when every word of the query, case-folded, occurs
in its name, id, package name or one of its keywords. AppStream's search also matches summaries
and descriptions, and over the thousands of components of Debian's DEP-11 catalogue such a hit
answers almost any query, arithmetic included: without the patch `12*7` offers Debian
applications whose description holds the characters. It is the rule Discover's own search follows (see
[Discover](../../../../docs/kdos/04-programs/discover.md)). The same patch narrows the runner's
other gate: it offers nothing when an installed application or command is named exactly what was
typed, case-insensitively, rather than on any `exec://` match another runner found. An application
counts when a match's text is the query (the services runner's KCalc for `kcalc`); a command counts
when the file name of a match's command, the first argument after `exec://`, is the query's first
word (the shell runner's "Run gimp" for `gimp`, "Run htop" for `htop`, whose text is never the
query). The services runner gives each application it matches an `exec://` id, its fuzzy matches
included, so under the unpatched gate `galculator` finds KCalc and no "Get galculator…" row
appears. It is to be offered upstream.

`appstream-runner-icon.patch` gives each "Get …" result of the Software Center runner an icon
it can draw. Upstream's `componentIcon()` adds a component's cached or local icon file whether or
not it exists, and tries a stock name, with `QIcon::fromTheme`, only while no file was added.
[debian-dep11](#port-debian-dep11) installs no icon files, so a Debian application such as
galculator got a QIcon that is not null and draws nothing, an empty slot, and one whose stock name
Breeze lacks, such as xdrawchem, got a placeholder. The patch adds a file only when it exists,
takes the first stock name `QIcon::hasThemeIcon()` knows, and with neither takes the icon of the
component's first main desktop-entry category, from the table
[kdos-discover](#port-kdos-discover)'s backend files a Debian application by
(`applications-utilities` for Utility, `applications-science` for Science, `kcolorchooser` for
Graphics), and only then `package-x-generic`. KRunner and Discover then show one icon for a Debian
application. It is to be offered upstream.

`desktop-view-title.patch` titles each desktop window with the shell package's name alone,
"Desktop", the title `shell/desktopview.cpp` already gives the view before it follows a screen.
Upstream's `DesktopView::setScreenToFollow()` appends `@` and a `QDebug` dump of the screen's
geometry. A screen reader takes the window title for the desktop's name, so Orca says "Desktop @
QRect(0,0 1920x1080)" each time focus returns to the desktop; a Qt debug string is no name. The
screen a view follows stays readable from `screenToFollow()`, and nothing on KDOS matches on the
title. It is to be offered upstream.

`timezone-selector-width.patch` sizes the floating toolbar under the time-zone map, in
`components/timezoneselector/TimezoneSelector.qml`, to its row and its padding. Upstream sets the
toolbar's width to the row's implicit width, so the row is laid out that width less the toolbar's
left and right padding; the two combo boxes do not shrink below their implicit width, and the
time-zone box overhangs the panel's right edge in plasma-setup's Time and Date page and wherever
else the selector is used. The patch drops the width binding, so the toolbar takes its implicit
width, and keeps the height binding. It gives the time-zone box the right margin the region box
already has, so the row's two ends are the same distance from the panel's edges with and without
the time-zone box shown. It is to be offered upstream.

`kcm-colors-accent-names.patch` gives each accent colour swatch on the Colors settings page, in
`kcms/colors/ui/AccentColorUI.qml`, a spoken name. The swatches are radio buttons with no text,
built from a model of ten bare hex strings, and the colour-picker button beside them carries only
its icon, so Orca reads each swatch as "not selected radio button" and the picker as an unnamed
button. The patch adds a list of ten translated colour names in model order (Pink, Red, Orange,
Yellow, Green, Teal, Blue, Purple, Violet, Gray) and sets each swatch's accessible name and
tooltip from it; the custom-colour indicator is named "Custom accent color" and the picker
"Choose a custom accent color…", which is also its tooltip. It adds no colour. It is to be offered
upstream as a merge request against plasma/plasma-workspace.

## Notes

`build.sh` builds the two generated headers `shell/qwayland-plasma-shell.h` and
`shell/wayland-plasma-shell-client-protocol.h` before the rest of the tree. They are generated for
the `plasmashell_osd` target and included by `plasmashell`'s own sources, and the build records no
dependency between the two, so a parallel build can compile `plasmashell` first and stop on a
missing header
([plasma-workspace#132](https://invent.kde.org/plasma/plasma-workspace/-/issues/132)). Alpine
builds the same two targets first.

The install puts the session's systemd user units under `/usr/lib/systemd`, which the package lint
refuses (see
[What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses));
`build.sh` removes the directory. Without the units the session takes upstream's classic path:
`startplasma-wayland` runs `plasma_session`, which starts `ksmserver`, `kded6` and the XDG
autostart entries, plasmashell's among them. With `startkderc`'s `[General] systemdBoot` unset,
`startplasma-wayland` first asks the session bus whether systemd's `plasma-workspace.target`
exists, and takes the classic path when the call fails; KDOS ships
`/usr/share/kdos/xdg/startkderc` with `systemdBoot=false`, outside this package, so the question is
never asked.

`build.sh` deletes the kded module `oom_notifier` and its `oom_notifier.notifyrc`. The module tells
the user about an application the kernel killed for memory by watching systemd's
`org.freedesktop.systemd1` scopes, which do not exist on KDOS, so it would load in every session
and never fire; the build has no switch for it. [kdos-oomd](#port-kdos-oomd) and
[kdos-notice](#port-kdos-notice) do that job here.

Without a service manager nothing restarts plasmashell after it exits, and it exits on purpose when
its compositor goes away: `shell/main.cpp` unsets `QT_WAYLAND_RECONNECT` for itself, and upstream's
user unit, which `build.sh` removes, carries the `Restart=on-failure` that would bring it back. The
module of [kded-kdos](#port-kded-kdos) starts it again instead (see
[How plasmashell is kept running](../../../../docs/kdos/04-programs/plasma-shell.md#how-plasmashell-is-kept-running)).

`startplasma-wayland` sets `XDG_MENU_PREFIX=plasma-`, so inside a session KService builds the
application tree from `/etc/xdg/menus/plasma-applications.menu`. A program whose environment lacks
the prefix, such as one run by `sudo` with a reset environment or from a timer, reads
`applications.menu`, which no other package installs; KService then builds an empty tree and "Open
With" lists nothing. `build.sh` links `applications.menu` to `plasma-applications.menu`, as Chimera
does.

The Users module hashes a new password itself and hands the hash to `accounts-daemon`. With
libxcrypt, which is not a port, it would ask `crypt_gensalt()` for the system's preferred method;
without it the module makes a `$6$` (SHA-512) salt of 16 random characters, which musl's `crypt()`
implements. It enrols fingers through [fprintd](#port-fprintd) on the system bus, and shows its
fingerprint controls only when fprintd reports a reader.

Three optional packages are looked for with `pkg-config` and no switch can refuse them: `flatpak`
(notification grouping for Flatpak applications), `systemd` (the autostart module's journal view of
a unit, and the systemd start-up targets) and `libxcrypt`. None is a port, so none is found. The
configure also reports two run-time packages: `kio-fuse`, which it looks for in the build root
only, and `appmenu-gtk-module`, the GTK module through which `gmenu-dbusmenu-proxy` receives a GTK
program's menu bar. The second is not a port, so a GTK program keeps its menu bar in its own
window.

[kde-cli-tools](#port-kde-cli-tools) and [kactivitymanagerd](#port-kactivitymanagerd) are programs
the session runs; [milou](#port-milou) and [qt6-qt5compat](#port-qt6-qt5compat) provide QML modules
that plasmashell and KRunner import; [kio-extras](#port-kio-extras) draws the wallpaper chooser's
thumbnails and [kio-fuse](#port-kio-fuse) gives a program that is not a KDE one a local path for a
remote file; [plasma-integration](#port-plasma-integration) and [breeze](#port-breeze) are the Qt
platform theme and the widget style and colours that the Breeze look-and-feel package names;
[tzdata](#port-tzdata) and [musl-locales](#port-musl-locales) are what the clock and Region &
Language read. The configure checks none of them, so each is in `depends` for run time.
