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
files. Upstream reads `/usr/share/i18n/locales` and has no option for the directory; there the
module finds no file, lists no format locale and reports that it could not list them. Chimera Linux
carries the same one-line change (cports `main/plasma-workspace/patches/locales.patch`).

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
