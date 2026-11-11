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
- `-D JS_PROVIDER=qml` — the JavaScript engine behind `syncthingctl edit --script` and
  `--js-lines`, which run a script over Syncthing's configuration instead of opening an editor,
  taken from Qt QML ([qt6-qtdeclarative](#port-qt6-qtdeclarative)). This is upstream's default.
- `-D WIDGETS_GUI=ON` — the Qt Widgets interface, with `-D QUICK_GUI=OFF` leaving out the
  experimental Qt Quick one; both are upstream's desktop defaults.
- `-D NO_LIBSYNCTHING=ON` — do not build Syncthing itself into the program as a library; the tray
  talks to the separate `syncthing` the Syncthing port installs. Upstream's default on the desktop.
- `-D NO_PLASMOID=OFF` — and `-D NO_FILE_ITEM_ACTION_PLUGIN=OFF`: build the Plasma panel widget
  and the Dolphin context-menu plugin, both upstream's defaults on Linux, named so neither drops
  out. The widget links [libplasma](#port-libplasma) and [kconfig](#port-kconfig) and shows the
  same folders, devices and transfers as the tray icon; the first-run wizard recommends it on
  Plasma, and it is added from the panel's widget list. The context-menu plugin is a KIO
  file-item-action plugin over [kio](#port-kio), which [Dolphin](#port-dolphin) loads to offer
  rescanning and opening a folder Syncthing shares.
- `-D SETUP_TOOLS=OFF` — no setup tools: the updater that checks for a newer Syncthing Tray
  release, downloads and verifies it and restarts into it, with its settings page and tray
  notice. This is upstream's default, and its description of the option says it likely makes
  sense to disable when distributing through a package manager; here kpkg installs and updates
  the program.
- `-D SYSTEMD_SUPPORT=OFF` — no control of a Syncthing systemd unit. KDOS has no systemd (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)), so there is no unit
  to control.
- `-D USE_BOOST_PROCESS=OFF` — launch Syncthing through Qt's `QProcess` instead of Boost.Process,
  which upstream enables by default.
- `-D DBUS_BASED_POWER_MONITORING=ON` — battery and power-saver monitoring over D-Bus, in
  upstream's words. On by default upstream on Linux.
- `-D NO_DOXYGEN=ON` — and `-D NO_SPHINX=ON`: no API documentation with Doxygen or Sphinx.

## Patches

`plasmoid-no-quick-gui.patch` guards the one use of the Qt Quick interface in the Plasmoid's
`showIgnorePatterns()` with the same `SYNCTHINGWIDGETS_GUI_QTQUICK_MODE_DESKTOP` check as every
other use in the applet, so the Plasmoid compiles with `-D QUICK_GUI=OFF` and opens the Widgets
ignore-patterns dialog. It is to be offered upstream.

## Notes

Upstream installs the logo as SVG only. The recipe rasterises it to 48, 64 and 128 pixel PNGs and
installs the 256-pixel PNG the source carries under `tray/resources/icons`, because the panel
draws PNG icons only. Upstream's desktop entry has no `StartupWMClass`; the recipe writes its
own, naming the window's app id, `syncthingtray`, the program's name.
