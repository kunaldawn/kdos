## About

KWindowSystem is the KDE Frameworks library through which a Qt program asks the windowing system
about its windows: activating one, the stacking order, virtual desktops, and on Wayland the
`xdg-activation` token one program hands another so the window it opens may take focus. Its X11
and Wayland halves are plugins, loaded for the platform the program runs on. [kio](#port-kio) and many KDE
applications link it.

## Flags

- `-D KWINDOWSYSTEM_WAYLAND=ON` — Build the Wayland platform plugin. It is upstream's default; on,
  the configure requires [qt6-qtwayland](#port-qt6-qtwayland),
  [wayland-protocols](#port-wayland-protocols) and
  [plasma-wayland-protocols](#port-plasma-wayland-protocols).
- `-D KWINDOWSYSTEM_X11=ON` — Build the X11 platform plugin, used by a KDE program that runs under
  Xwayland. It is upstream's default and requires [libX11](#port-libX11) and the xcb keysyms and
  ICCCM utility libraries ([xcb-util-keysyms](#port-xcb-util-keysyms),
  [xcb-util-wm](#port-xcb-util-wm)). Every toolkit in the tree keeps both backends (see
  [Toolkits, Wayland and X11](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/applications.md#toolkits-wayland-and-x11)).
- `-D KWINDOWSYSTEM_QML=ON` — Build the QML module, `org.kde.kwindowsystem`, for Qt Quick
  programs. It is upstream's default and needs [qt6-qtdeclarative](#port-qt6-qtdeclarative).
