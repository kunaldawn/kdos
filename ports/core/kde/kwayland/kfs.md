## About

KWayland is a Qt-style client library for Wayland: one class per protocol object, signals for
its events, and a connection thread that dispatches them. Beside the standard protocols it binds
Plasma's own, from [plasma-wayland-protocols](#port-plasma-wayland-protocols): the shell's panel
and window-management interfaces, blur, background contrast, slide effects, the decoration
palette, DPMS, fake input and the application menu. [kwin](#port-kwin), the Plasma shell and
the KDE desktop portal link it; the server side of these protocols lives in KWin itself.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Passed as by every KDE port, so a later release that adds
  translations stays English-only with no change to the recipe (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  This release carries none.
- `-D BUILD_QCH=OFF` — No API documentation in Qt Help format; no port builds QCH.

## Notes

The version is Plasma's, and 6.8 consumers ask for 6.8.0 or later. The configure requires
`plasma-wayland-protocols` 1.22.0 or later, EGL through [libglvnd](#port-libglvnd), and Qt's
private GUI and Wayland client API, which carries no compatibility promise between Qt releases,
so this port is rebuilt with every [qt6-qtwayland](#port-qt6-qtwayland) bump.
