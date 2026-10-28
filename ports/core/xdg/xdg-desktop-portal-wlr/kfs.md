## About

xdg-desktop-portal-wlr is the [screen-capture portal]{.idx} backend for wlroots compositors: it
implements the ScreenCast interface of `xdg-desktop-portal` by capturing screens and windows
through the compositor's `ext-image-copy-capture` and `wlr-screencopy` globals and handing the
frames to PipeWire, and the Screenshot interface by running [grim](#port-grim), with
[slurp](#port-slurp) when the request asks the user to pick a region or a colour. The main portal
has no capture code of its own, so this backend is what answers every program that records the
screen through the portal: OBS, a browser, a boxed application, and `kdos-record`, the desktop's
screen recorder, which depends on it. Tools that speak the capture protocols to the compositor
themselves, such as `grim` run on its own and `wf-recorder`, do not go through it.

Two things about how KDOS runs it come from outside the recipe. `kdos-portals.conf` names it as
the backend for ScreenCast and Screenshot, because the `wlr.portal` file it installs offers
itself only to other desktops' names. And `kdos-desktop-start` asks the bus to start it only
once the compositor's socket exists and `WAYLAND_DISPLAY` is in the bus's activation environment,
and only then starts the main portal, which reads its backends once at startup; activated by a
client's first call before that, the backend would start with no `WAYLAND_DISPLAY` and exit.
Both are in [Portals](../../../../docs/kdos/03-architecture/session.md#portals) and
[Startup ordering](../../../../docs/kdos/03-architecture/session.md#startup-ordering).

## Flags

- `--libexecdir=/usr/lib` — Install the daemon as `/usr/lib/xdg-desktop-portal-wlr` instead of
  meson's default `/usr/libexec`. The same directory goes into the `Exec=` line of the D-Bus
  service file it installs, which the bus runs, and `kdos-desktop-start` looks for the daemon at
  that path before it asks the bus to start it.
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
