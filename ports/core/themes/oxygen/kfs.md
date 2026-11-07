## About

Oxygen is KDE's look from the KDE 4 years, kept as an alternative to Breeze. The port installs the
Oxygen widget style for Qt 6 applications with its settings program `oxygen-settings6` and
`oxygen-demo6`, the Oxygen window decoration for KWin, six cursor themes (`Oxygen_Black`, `_Blue`,
`_White`, `_Yellow`, `_Zion` and `KDE_Classic`), three colour schemes, the Oxygen and Air Plasma
styles, the Air and Horos wallpapers, and three global themes, Oxygen, Oxygen Light and Air, which
set all of these at once. Nothing on KDOS selects it; it is there to be chosen in System Settings.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the translations of the style's and the
  decoration's settings, which the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_QT5=OFF` — No Qt 5 widget style. Upstream builds one by default, and the Qt 5 build
  needs KDE Frameworks 5, which are not ports; Qt 5 applications are themed
  through [qt5ct](#port-qt5ct) instead.
- `-D BUILD_QT6=ON` — The Qt 6 style and everything else in the package, upstream's default.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_XCB=ON` — Make [libxcb](#port-libxcb) a requirement. The style
  and the decoration use it to pass their properties to a window that runs on the X11 platform,
  which on KDOS is a Qt program under Xwayland; upstream looks for it quietly and drops that code
  when it is absent.

## Notes

The global themes are Plasma packages installed with libplasma's CMake macros, so
[libplasma](#port-libplasma) is a requirement of the configure. The Plasma styles' SVGs are
installed compressed as `.svgz`, made with `gzip -n` from [gzip](#port-gzip), which stores no
name and no time, so the package is the same on every build. The cursor themes ship as finished
XCursor files in the source and are installed as they are.
