## About
Qt Wayland for Qt 5: the `wayland` platform plugin that runs a Qt 5 program as a native Wayland
client, with its EGL integration and its shell integrations, xdg-shell among them, and the
QtWaylandCompositor module for writing a compositor in Qt. The one display-server plugin
[qt5-qtbase](#port-qt5-qtbase) builds is `xcb`, so without this port every Qt 5 program runs under
Xwayland. The session's `QT_QPA_PLATFORM="wayland;xcb"` makes the Wayland plugin the first choice
(see [Starting a session](../../../../docs/kdos/03-architecture/session.md#starting-a-session)).

## Flags
- `-feature-wayland-client` — with `-feature-wayland-client-xdg-shell` and
  `-feature-wayland-egl`: the client plugin, the xdg-shell integration through which a window gets
  a title bar and a place on the desktop, and the EGL integration that lets it draw with OpenGL,
  each named so that a missing library stops the configure.
- `-feature-wayland-server` — the QtWaylandCompositor module. Upstream's default, named.
- `-no-feature-xcomposite-egl` — with `-no-feature-xcomposite-glx`: no XComposite hardware
  integrations, which hand a client's buffers to a Qt compositor as X11 pixmaps. Upstream builds
  them whenever `libXcomposite` is found, and it is not in `depends`.
