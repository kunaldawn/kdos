## About

Falkon is KDE's web browser, built on Qt WebEngine ([qt6-qtwebengine](#port-qt6-qtwebengine)),
the Chromium engine packaged as a Qt module. On KDOS it carries its KDE Frameworks integration
plugin, which keeps passwords in KWallet, opens KIO's URL schemes, reports downloads as KDE jobs
and adds a *Share page* menu through Purpose, and its Python plugin support through PySide6.

## Flags

- `-D BUILD_TESTING_ONLINE=OFF` — Build none of the tests that need a network connection;
  upstream's default.
- `-D KF_SKIP_PO_PROCESSING=ON` — `ki18n_install(po)` returns at once, so none of the message
  catalogues in `po/` is compiled or installed. kpkg removes every non-English catalogue from the
  package anyway (see [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_KEYRING=OFF` — No GNOME Keyring password plugin. It needs the `gnome-keyring-1`
  library, which is not a port; passwords go to KWallet instead.
- `-D BUILD_PYTHON_SUPPORT=ON` — Python plugins, through PySide6. On by default; the three
  PySide6 and Shiboken6 requirements in this list make it fail the configure rather than drop out.
- `-D NO_X11=OFF`, `-D DISABLE_DBUS=OFF` — Keep Falkon's X11 code and its D-Bus support. Both are
  upstream's defaults. The X11 code is xcb calls that set the window's `WM_CLASS` and read and set
  its workspace (`_NET_WM_DESKTOP`), made only when Qt runs Falkon on its `xcb` platform, under
  Xwayland. It is why [libxcb](#port-libxcb) and [xcb-util](#port-xcb-util) are in `depends`:
  with it on, configure requires both.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Wallet=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6KIO=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6CoreAddons=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Purpose=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6JobWidgets=ON`
  — Falkon looks for these six frameworks as optional and builds the KDE Frameworks integration
  plugin only when all six are found. Required, a missing one stops the configure instead of
  quietly dropping the plugin (see
  [a graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_PySide6=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_Shiboken6=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_Shiboken6Tools=ON` — The same for the Python plugin support,
  which is built only when PySide6, Shiboken6 and its tools are all found
  ([python3-pyside6](#port-python3-pyside6)).

## Patches

- `app-id.patch` — Edits `linux/applications/org.kde.falkon.desktop`. It changes `StartupWMClass=Falkon` to `StartupWMClass=org.kde.falkon`, the entry's own desktop id, so the panel's lookup by file name and its lookup by window class find the one entry. It also deletes the `MimeType=` line, which claimed `text/html`, `application/xhtml+xml`, `http`, `https` and `application/x-mimearchive`: the web types are Firefox ESR's, their default in `/etc/xdg/kdos-mimeapps.list`, and an entry claims a type only where nothing else does (see [desktop entries](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).
