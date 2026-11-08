## About

fcitx5-qt is [fcitx5](#port-fcitx5)'s Qt 6 side: the input-context plugin `fcitx5` that a Qt
program loads when `QT_IM_MODULE=fcitx` is set, and the two libraries Fcitx's own Qt tools are
built on, `Fcitx5Qt6DBusAddons` (the D-Bus client for the running fcitx5) and
`Fcitx5Qt6WidgetsAddons` (the key-sequence and key-list widgets). It also installs the Quick
Phrase editor and `fcitx5-qt6-gui-wrapper`, which open an addon's own configuration window.

Under Plasma, KWin starts fcitx5 as the session's input method and a Qt 6 program reaches it
through the compositor's `text-input-v3`, with `QT_IM_MODULE` unset (see
[Input methods](../../../../docs/kdos/03-architecture/session.md#input-methods)). The plugin is
for a program that sets the variable itself; the libraries are what
[fcitx5-configtool](#port-fcitx5-configtool), the System Settings page, links.

## Flags

- `-D ENABLE_QT4=OFF` — With `-D ENABLE_QT5=OFF`: build for Qt 6 alone. There is no Qt 4 in the
  tree, and a Qt 5 plugin would make [qt5-qtbase](#port-qt5-qtbase) a dependency of the input
  method for the Qt 5 programs that already reach fcitx5 over `text-input-v2`.
- `-D ENABLE_QT6=ON` — The Qt 6 plugin and libraries, the whole of this package.
- `-D ENABLE_X11=OFF` — Build the plugin without its xcb code, which reads the X keyboard state
  and window positions for a Qt program on the `xcb` platform. fcitx5 itself is built without
  X11, so a program running under Xwayland has no input method either way.
- `-D ENABLE_QT6_WAYLAND_WORKAROUND=ON` — Upstream's default, named because it adds
  [qt6-qtwayland](#port-qt6-qtwayland)'s client and private headers to the requirements: the
  plugin then asks the Wayland platform for the focused surface, so the candidate window is
  placed at the caret.

## Notes

The source is a `.tar.zst`, which `kpkg` copies into the work directory whole instead of
unpacking; `build.sh` unpacks it, as the other fcitx5 recipes do.

The project includes `GNUInstallDirs` itself and never reads ECM's `KDE_INSTALL_*` names, so the
library directory is passed as `CMAKE_INSTALL_LIBDIR`.
