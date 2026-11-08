## About

Plasma Desktop is the desktop shell package that [plasma-workspace](#port-plasma-workspace)'s
`plasmashell` loads: the folder-view desktop and panel containments, Kickoff and the other
launchers, the task manager, the pager, the emoji selector, the input-method panel, the device
automounter, and the System Settings modules for keyboard, mouse, touchpad, touchscreen, tablet,
game controllers, shortcuts, date and time, accessibility, activities, search, session start and
the splash screen. It also installs SDDM's Breeze login theme in `/usr/share/sddm/themes/breeze`
and KDE's default `kde-mimeapps.list`.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compile none of the interface translation catalogues in `po/`.
  kpkg removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Baloo=ON` — The File Search settings module is built only when
  [baloo](#port-baloo) is found, which upstream reports as recommended and does not require.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_SDL2=ON` — The Game Controller settings module is built only when
  SDL 2 is found; [sdl2-compat](#port-sdl2-compat) provides it over SDL 3.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_UDev=ON` — The Game Controller module lists devices through
  libudev from [eudev](#port-eudev), which upstream looks for quietly.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_XdgUserDir=ON` — The Paths settings module writes the Desktop,
  Documents and other folders to `~/.config/user-dirs.dirs`, the file
  [xdg-user-dirs](#port-xdg-user-dirs) creates at login, and names the defaults after
  xdg-user-dirs' own. Upstream looks for its `xdg-user-dir` program as a run-time dependency and
  only reports its absence.
- `-D CMAKE_DISABLE_FIND_PACKAGE_AccountsQt6=ON` — With
  `-D CMAKE_DISABLE_FIND_PACKAGE_KAccounts6=ON`: no OpenDesktop integration plugin, which signs in
  to the KDE Store through the accounts-sso stack. That stack,
  [libaccounts-qt](#port-libaccounts-qt) and
  [kaccounts-integration](#port-kaccounts-integration), is not in `depends`, and the plugin also
  needs signon's OAuth2 plugin at run time; refusing both keeps the package the same whichever of
  them the build root holds.
- `-D CMAKE_DISABLE_FIND_PACKAGE_PackageKitQt6=ON` — No PackageKit integration, which the KRunner
  plugin installer uses to install a downloaded plugin's package. PackageKit is not a port.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Patches

`sandbox-badge.patch` puts the sandbox a window's application runs in at the top of its Task
Manager tooltip's subtext: "In box: www" for a KDOS box, "Flatpak: <id>" for a Flatpak, "In sandbox:
<name>" for any other engine, per window in a grouped tooltip. It is QML only. It reads
libtaskmanager's `SandboxEngine` and `SandboxName` roles from
[plasma-workspace](#port-plasma-workspace)'s patch of the same name; a libtaskmanager without them
reads as empty and the tooltip draws no line. It is proposed upstream with it.

## Notes

Two of the settings modules talk to system services that this package does not provide. Date & Time
saves the time, the time zone and NTP through `org.freedesktop.timedate1`, which
[kdos-settingsd](#port-kdos-settingsd) serves; without that service on the system bus the module
shows its settings and cannot apply any.
The Users module, which edits accounts through [accountsservice](#port-accountsservice), is
plasma-workspace's.

The Tablet settings module, on by default, makes [libwacom](#port-libwacom) and
[wayland-protocols](#port-wayland-protocols) requirements of the configure, and the keyboard module
needs `xkeyboard-config`'s data directory, found through `pkg-config` from
[xkeyboard-config](#port-xkeyboard-config). [libX11](#port-libX11) is required: `kaccess`, the
accessibility daemon behind sticky and slow keys and the visual bell, is built only with it.

The input-method panel has backends for IBus and SCIM, each built when `pkg-config` finds the
library; neither is a port, and [fcitx5](#port-fcitx5) talks to the panel over D-Bus with no
backend of its own. The emoji selector draws with [noto-emoji](#port-noto-emoji), which upstream
only reports at configure time. [kactivitymanagerd](#port-kactivitymanagerd), which the activity
settings and the pager talk to, and [kpipewire](#port-kpipewire), whose QML module the task
manager's window thumbnails import, are in `depends` for run time.

`kaccess` installs a systemd user unit under `/usr/lib/systemd`, which the package lint refuses
(see
[What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses));
`build.sh` removes the directory. Its XDG autostart entry, phase 0, is what starts it in a Plasma
session.
