## About

`kdos-comp` is the KDOS [compositor]{.idx}: the Wayland display server that owns the screen, the
input devices and every window. It is a hard fork of the labwc compositor, built on
[wlroots](#port-wlroots); labwc's core does the window management, and KDOS's additions sit in
files of their own beside it: the phosphor pass, the wallpaper, idle and lock, window groups and
memory, box identity, accessibility, motion, the supervised session children and the command and
frame sockets. It runs Xwayland for X11 clients, relays input methods between
[fcitx5](#port-fcitx5) and applications, and starts the panel and the other pieces of the
session. Its chapter is [kdos-comp](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/kdos-comp.md).

Because it is a fork rather than a set of patches, the source lives in this directory, with
`KDOS-FORK` naming the upstream tarball it was taken from and its checksum, and there is no
`source =` line to fetch. The recipe is upstream's meson build, run from `$PORT_SRC` into a build
directory in the work tree, with three KDOS libraries added through the environment.

## Flags

- `-Dxwayland=enabled` — Builds X11 support, and makes its absence a configure error: meson stops
  unless `xwayland.pc`, `xcb`, `xcb-ewmh` and `xcb-icccm` are found and [wlroots](#port-wlroots)
  was itself built with Xwayland support. With `auto` the compositor would build without X11,
  with at most a warning. Xwayland is the one X server KDOS has; see
  [Xwayland](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/kdos-comp.md#xwayland).
- `-Dicon=disabled` — Builds without application icons from desktop entries and icon themes
  (in the window switcher, the menus and title bars). Icons need upstream's libsfdo libraries,
  which are not a port, and with the option on they are required.
- `-Dsvg=disabled` — Uses no SVG images for title-bar buttons, so the compositor does not link
  [librsvg](#port-librsvg). Upstream's default would link it whenever it is installed, and it is
  installed for [fcitx5](#port-fcitx5) and others.
- `-Dnls=disabled`, `-Dman-pages=disabled`, `-Dlabnag=disabled`, `-Dsystemd-session=disabled` —
  Upstream's translations, manual pages, `labnag` dialog and systemd session target. The fork
  carries no `po/`, `docs/`, `clients/` or `data/` directory, and its `meson.build` reads none of
  these four options, so in this build they change nothing; meson accepts them because
  `meson_options.txt` declares them.

## Notes

`build.sh` compiles `libkbase`, `libkcolor` and `libkwm` from `src/libs/` into a static archive,
`klibs/libkdos.a`, before meson runs. Meson reads `CFLAGS` and `LDFLAGS` from the environment
when it sets up the build, so the archive reaches the link through `LDFLAGS` and the three
libraries' headers through `CFLAGS`, both exported with the tree's own flags kept in front.
`libkcolor` supplies the colour schemes that the phosphor pass, the wallpaper, the box chips and
the configuration reader use, and `libkwm` is the window model, placement, tiling and the edge search, kept in a library so that it
can be tested against a fixture with no compositor running. See
[How the libraries are built](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).

Editing any of those libraries changes the compositor, and a port under `src/` hashes all of
`src/libs/` as part of its recipe, so editing any library at all rebuilds it. To rebuild only the
compositor, `make build BUILD_ARGS="--phases 50_desktop --rebuild kdos-comp"`; the checks that
cover it without a full build are in
[Building and testing](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/kdos-comp.md#building-and-testing).

The manual pages are not installed, so `man labwc-config` finds nothing on a KDOS machine; labwc's
own documentation applies to `rc.xml`, `menu.xml` and the theme as written, with the differences
listed in
[How labwc's documentation applies](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/kdos-comp.md#how-labwcs-documentation-applies).
