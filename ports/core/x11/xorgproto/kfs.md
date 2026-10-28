## About

xorgproto is the set of X Window System protocol headers, the core protocol and its extensions,
with a pkg-config file for each. It installs no code. Every X client library builds against it,
[libxcb](#port-libxcb) and the `libX*` libraries among them, and so does
[Xwayland](#port-xwayland), the one X server in KDOS (see
[No Xorg server, and one carve-out](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out)).

## Flags

- `--datadir=/usr/share` — Where the pkg-config files go: upstream installs them under
  `datadir/pkgconfig`, as files that are the same on every architecture, so they land in
  `/usr/share/pkgconfig`, which pkgconf searches beside `/usr/lib/pkgconfig`. `/usr/share` is
  also meson's default for this prefix.

## Notes

meson's `legacy` option stays at its default, off, so the headers and pkg-config files of the
obsolete extensions that upstream keeps only under that option are not installed.
