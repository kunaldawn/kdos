## About

xsettingsd serves XSETTINGS, the X11 protocol through which a GTK program learns the theme, icon
theme, fonts, font rendering and cursor. A GTK program running under Xwayland cannot read them
from the Wayland session, so without it every X11 GTK window draws GTK's defaults. Plasma's
[kde-gtk-config](#port-kde-gtk-config) starts it inside the session and writes
`~/.config/xsettingsd/xsettingsd.conf` from the settings it applies; nothing else on KDOS runs it.

## Notes

The install puts a systemd user unit, `xsettingsd.service`, under `/usr/lib/systemd/user`, with
no switch to skip it. `build.sh` removes the directory, and then `/usr/lib`, which nothing else in
the package uses, so the package does not carry an empty directory (see
[No systemd init, units or service manager](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)).

The tests need googletest and are built only with `BUILD_TESTING` on, so `-D BUILD_TESTING=OFF`
keeps the configure from looking at whatever googletest the build root holds. `dump_xsettings`,
which prints what the running X server's settings owner serves, is installed beside the daemon.
