## About

fcitx5-configtool is [fcitx5](#port-fcitx5)'s settings interface. Its `kcm_fcitx5` is the
**Input Method** page of System Settings: which engines are active, their order, the global keys
and each addon's options. It also installs `fcitx5-config-qt`, the same settings as a standalone
window that fcitx5's own "Configure" action opens; `kbd-layout-viewer5`, which draws a keyboard
layout; `fcitx5-plasma-theme-generator`, which writes a fcitx5 classic-UI theme from the Plasma
style; and `fcitx5-migrator`, which imports Fcitx 4 settings. The input method itself is started
by KWin, which System Settings' Virtual Keyboard page points at fcitx5 (see
[Input methods](../../../../docs/kdos/03-architecture/session.md#input-methods)).

## Flags

- `-D ENABLE_KCM=ON` — Build the System Settings page. It needs Kirigami,
  [libplasma](#port-libplasma), [ksvg](#port-ksvg), [kcmutils](#port-kcmutils) and the other
  frameworks in `depends`, all required by the configure once this is on.
- `-D ENABLE_CONFIG_QT=ON` — Build `fcitx5-config-qt`.
- `-D ENABLE_TEST=OFF` — Build no tests.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DBusAddons=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6WindowSystem=ON`: `fcitx5-config-qt` is a single-instance
  application only when both are found, and otherwise a second start opens a second window.
  Upstream looks for them quietly.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Svg=ON` — Qt's SVG support, which upstream marks a run-time
  dependency "used for icons": without [qt6-qtsvg](#port-qt6-qtsvg) the engine and layout icons
  in both interfaces are blank.

## Notes

The source is a `.tar.zst`, which `kpkg` copies into the work directory whole instead of
unpacking; `build.sh` unpacks it, as the other fcitx5 recipes do.

The keyboard-layout code reads layout names from [xkeyboard-config](#port-xkeyboard-config)'s
rules files and draws a layout through [libxkbfile](#port-libxkbfile) and Xlib's XKB headers; it
opens no X connection, but the configure requires [libX11](#port-libX11) and libxkbfile whatever
is built.

`fcitx5-migrator` imports Fcitx 4's configuration, which never exists on KDOS, so `build.sh`
marks its desktop entry `NoDisplay=true` with `desktop-file-edit` from
[desktop-file-utils](#port-desktop-file-utils), and it is not a row in the application menu. The
Input Method page's own entry and `fcitx5-config-qt`'s are hidden upstream, since System Settings
and fcitx5 open them.

ECM warns at configure time that each `KDE_INSTALL_*` directory took its value from the
`GNUInstallDirs` one, because fcitx5's CMake package includes `GNUInstallDirs` before
`KDEInstallDirs` runs; the two agree under `/usr`, so the warnings change nothing.
