## About

KIdleTime tells a program how long the user has been idle and wakes it when a given idle time
passes or the user comes back. It is how a session dims the screen, locks it or suspends, and how
a chat program marks itself away. The work is done by one poller plugin per platform: on Wayland
it asks the compositor through the `ext-idle-notify-v1` protocol (or KDE's older
`org_kde_kwin_idle`), under X11 through the X Sync or X Screen Saver extension. PowerDevil and
the screen locker of Plasma, [konversation](#port-konversation) and [baloo](#port-baloo) use it.

## Flags

- `-D WITH_WAYLAND=ON` — Build the Wayland poller. It is upstream's default; on, the configure
  requires [qt6-qtwayland](#port-qt6-qtwayland), [wayland](#port-wayland),
  [wayland-protocols](#port-wayland-protocols) and
  [plasma-wayland-protocols](#port-plasma-wayland-protocols).
- `-D WITH_X11=ON` — Build the X11 pollers, used by a program that runs under Xwayland. It is
  upstream's default; on, the configure requires [libX11](#port-libX11) and
  [libxcb](#port-libxcb), and stops unless it finds the X Sync extension of
  [libXext](#port-libXext) or the X Screen Saver one of [libXScrnSaver](#port-libXScrnSaver); both
  are in `depends`, so both pollers are built. Every toolkit in the tree keeps both backends (see
  [Toolkits, Wayland and X11](../../../../docs/kdos/02-user-guide/applications.md#toolkits-wayland-and-x11)).
