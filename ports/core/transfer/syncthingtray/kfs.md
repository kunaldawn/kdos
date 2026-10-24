## About

Syncthing Tray shows the state of a local [Syncthing](#port-syncthing) in the panel tray: which
folders and devices are in sync, what is transferring, and the recent changes, and it can pause,
rescan and restart Syncthing. It is a Qt 6 Widgets program built on Martchus's
[cpp-utilities](#port-cpp-utilities), [qtutilities](#port-qtutilities) and
[qtforkawesome](#port-qtforkawesome). The port also installs `syncthingctl`, the command-line
client the desktop entry's "Rescan all folders" action runs.

## Flags

- `-D BUILD_SHARED_LIBS=ON` — build the project's own libraries (connector, model, widgets) as
  shared libraries.
- `-D QT_PACKAGE_PREFIX=Qt6` — and `-D KF_PACKAGE_PREFIX=KF6`: the Qt and KDE Frameworks major
  version to build against; upstream's defaults are `Qt5` and `KF5`.
- `-D WEBVIEW_PROVIDER=none` — no built-in web view. Upstream's `auto` would embed Syncthing's web
  interface through QtWebEngine; with `none` the web interface opens in the browser, and
  [qt6-qtwebengine](#port-qt6-qtwebengine) is not linked.
- `-D JS_PROVIDER=qml` — the JavaScript engine of `syncthingctl`'s script console, from
  Qt QML ([qt6-qtdeclarative](#port-qt6-qtdeclarative)). This is upstream's default.
- `-D WIDGETS_GUI=ON` — the Qt Widgets interface, with `-D QUICK_GUI=OFF` leaving out the
  experimental Qt Quick one; both are upstream's desktop defaults.
- `-D NO_LIBSYNCTHING=ON` — do not build Syncthing itself into the program as a library; the tray
  talks to the separate `syncthing` the Syncthing port installs. Upstream's default on the desktop.
- `-D NO_PLASMOID=ON` — and `-D NO_FILE_ITEM_ACTION_PLUGIN=ON`: no Plasma panel applet and no
  Dolphin context-menu plugin, both on by default on Linux. The applet needs Plasma's applet
  library, which is not a port. The context-menu plugin is a KIO file-item-action plugin, which
  [Dolphin](#port-dolphin) would load; it is not built.
- `-D SETUP_TOOLS=OFF` — no setup tools; upstream's own description says this likely makes sense
  to disable when distributing through a package manager.
- `-D SYSTEMD_SUPPORT=OFF` — no control of a Syncthing systemd unit. KDOS has no systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)), so there is no unit
  to control.
- `-D USE_BOOST_PROCESS=OFF` — launch Syncthing through Qt's `QProcess` instead of Boost.Process,
  which upstream enables by default.
- `-D DBUS_BASED_POWER_MONITORING=ON` — battery and power-saver monitoring over D-Bus, in
  upstream's words.
- `-D NO_DOXYGEN=ON` — and `-D NO_SPHINX=ON`: no API documentation with Doxygen or Sphinx.

## Notes

Upstream installs the logo as SVG and a 256-pixel PNG; the recipe rasterises the SVG to 48, 64
and 128 pixel PNGs as well, because the panel draws PNG icons only. Upstream's desktop entry has
no `StartupWMClass`; the recipe writes its own, naming the window's app id, `syncthingtray`, the
program's name.
