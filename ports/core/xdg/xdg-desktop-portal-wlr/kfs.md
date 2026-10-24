## About

xdg-desktop-portal-wlr is the [screen-capture portal]{.idx} backend for wlroots compositors: it
implements the ScreenCast and Screenshot interfaces of `xdg-desktop-portal` by capturing outputs
through the compositor's `ext-image-copy-capture` and `wlr-screencopy` globals and handing the
frames to PipeWire. The main portal has no capture code of its own, so this backend is what
answers every program that records the screen through the portal: OBS, a browser, a boxed
application, and `kdos-record`, the desktop's screen recorder, which depends on it. Tools that
speak the capture protocols to the compositor themselves, such as `grim` and `wf-recorder`, do
not go through it.

Two things about how KDOS runs it come from outside the recipe. `kdos-portals.conf` names it as
the backend for ScreenCast and Screenshot, because the `wlr.portal` file it installs offers
itself only to other desktops' names. And `kdos-desktop-start` starts it directly once the
compositor's socket exists, rather than leaving it to D-Bus activation, whose environment has no
`WAYLAND_DISPLAY`. Both are in [Portals](../../../../docs/kdos/03-architecture/session.md#portals)
and [Startup ordering](../../../../docs/kdos/03-architecture/session.md#startup-ordering).

## Flags

- `--libexecdir=/usr/lib` — Install the daemon as `/usr/lib/xdg-desktop-portal-wlr` instead of
  meson's default `/usr/libexec`. The same directory goes into the `Exec=` line of the D-Bus
  service file it installs, and `kdos-desktop-start` runs the daemon by that path.
- `-Dsd-bus-provider=basu` — Take the sd-bus API from `basu`, the stand-alone extraction of
  systemd's bus library, rather than `libsystemd` or `libelogind`; KDOS has
  [no systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd). Left at `auto`, meson
  takes the first of the three it finds.
- `-Dsystemd=disabled` — Install no systemd user unit, and leave the `SystemdService=` line out of
  the D-Bus service file.
- `-Dman-pages=enabled` — Build `xdg-desktop-portal-wlr(5)` with `scdoc`, which is why `scdoc` is
  a dependency; `enabled` makes a missing `scdoc` a configure error. See
  [Manual pages](../../../../docs/kdos/05-developer/writing-ports.md#manual-pages).

## Notes

- **Its configuration is per user.** `/etc/skel/.config/xdg-desktop-portal-wlr/config` sets the
  output chooser (`fuzzel`, as a dmenu-style list that can offer windows as well as screens) and
  `max_fps=30`, which bounds what a recording of a still screen costs; see
  [Recording](../../../../docs/kdos/03-architecture/session.md#recording).
