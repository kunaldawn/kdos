## About

xdg-desktop-portal-kdos is KDOS's own [portal backend]{.idx}. It answers the FileChooser,
Settings, AppChooser and Access requests that `xdg-desktop-portal` forwards from applications,
boxed ones included, so an application's Open and Save dialog is the desktop's own chooser,
drawn in the desktop's palette and chrome with its file roots, and not whatever its toolkit draws.
Settings hands out the colour scheme and the accent. `xdg-desktop-portal-wlr` does ScreenCast and
Screenshot only, and `xdg-desktop-portal-gtk`, the backend other desktops fall back to, would put
GTK's own dialogs in front of these.

It is a bus adapter and nothing more: for a file choice it runs `kdos-pick` and reads its output,
for an access question it runs `kdos-prompt` and reads its exit status. Both are names of
`kdos-shell`, which is why that port is a dependency. Its rules (every request is answered, and the
bus loop never waits on a dialog) are in
[The KDOS backend](../../../docs/kdos/03-architecture/session.md#the-kdos-backend).

## Notes

- **Three installed files, each needed.** The program is `/usr/lib/xdg-desktop-portal-kdos`, the
  path the D-Bus activation file `kdos.service` names in `Exec=`; D-Bus starts it on the first
  request for `org.freedesktop.impl.portal.desktop.kdos`. `kdos.portal` lists the four interfaces
  and `UseIn=KDOS`. Without either file the front end never uses the backend. Which interface goes
  to which backend is decided by `/usr/share/xdg-desktop-portal/kdos-portals.conf`, from `fs/`; see
  [Portals](../../../docs/kdos/03-architecture/session.md#portals).
- **It links only `basu`.** The colour table comes from `libkcolor`'s `kcolor.h`, which `main.c`
  includes by a path relative to itself, so the compile line needs no `-I` for it and compiles none
  of `libkcolor`'s code; see
  [How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).
