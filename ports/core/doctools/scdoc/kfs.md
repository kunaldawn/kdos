## About

scdoc turns manual pages written in its own small markup into roff. Projects from the Wayland and
sway world write their pages this way, so scdoc is a build dependency of `foot`, `seatd`, `fuzzel`,
`grim`, `slurp`, `waypipe`, `xdg-desktop-portal-wlr` and others, and of `kmod` and `muon`. It is
one C program with no dependencies and no configure script: a plain Makefile builds it.

It also installs `scdoc.pc`, whose `scdoc` variable names the program. A meson project finds scdoc
with `dependency('scdoc', native: true)` and runs the program that variable names (seatd's
`meson.build` reads it with `get_variable(pkgconfig: 'scdoc')`), so the path written there is the
one every such build runs.

## Flags

- `PREFIX=/usr` — The Makefile's install prefix, `/usr/local` unless it is given. It is passed to
  both `make` calls, because each uses it: the first writes it into `scdoc.pc` as that file is
  generated, and without it there `scdoc.pc` would name `/usr/local/bin/scdoc`; the second derives
  `BINDIR`, `MANDIR` and the pkg-config directory from it for the install.
