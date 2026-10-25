## About
[sc-im]{.idx} is a spreadsheet for the terminal with vi-style keys, descended from `sc`. Because a
spreadsheet is a grid of cells, it runs at the console font with nothing lost. It reads and writes
its own `.sc` files and CSV, reads `.xlsx` and `.ods` through [libxml2](#port-libxml2) and
[libzip](#port-libzip), writes `.xlsx` through [libxlsxwriter](#port-libxlsxwriter), runs Lua
scripts, and plots through [gnuplot](#port-gnuplot).

## Flags
- `-Wno-error` — exported in `CFLAGS`. It turns any `-Werror` back into plain warnings; this
  release's makefile adds none of its own.
- `LUA_PKGNAME=lua` — the `pkg-config` name of the Lua that scripting links. Left unset, the
  makefile takes the highest-versioned `luaX.Y` name `pkg-config` lists, so the result would follow
  which of [lua](#port-lua) and [lua54](#port-lua54) happened to be installed. `lua` is the lua
  port's own `.pc` file, which lua54 does not install.
- `CPPFLAGS=-g0` — the makefile adds `-g` to `CFLAGS`, and make's built-in compile rule puts
  `CPPFLAGS` after `CFLAGS`, so `-g0` cancels it and no object carries debug information.
- `THEMEDIR=/usr/share/sc-im/themes` — install the colour themes beside sc-im's other data.
  Upstream's default is `$(prefix)/share/themes`, the directory that holds desktop toolkit
  themes.

## Notes
The `CFLAGS` are exported, never passed on the `make` command line. The makefile builds its whole
configuration with `CFLAGS +=` (the help path, the configuration directory, the history file, the
ncurses and colour switches and every other `-D` the source reads), and a command-line
assignment would replace all of it; the build then fails on `'CONFIG_DIR' undeclared`. Exported,
the tree's flags are the base and the makefile appends to them. For the same reason the clipboard
commands cannot be set from outside, which is what `wayland-clipboard.patch` is for: `:ccopy` and
`:cpaste` go through `wl-copy` and `wl-paste` from [wl-clipboard](#port-wl-clipboard) instead of
`xclip`.

The `.xlsx` and `.ods` readers, the `.xlsx` writer, Lua and plotting are all probes with no flag:
the makefile asks `pkg-config` for `libxml-2.0` and `libzip`, for `xlsxwriter` and for the Lua
package, and defines `GNUPLOT` only when `which gnuplot` answers at build time. A failed probe
leaves the feature out of a build that otherwise succeeds, which is why each of those is in
`depends`. Legacy `.xls` import needs libxls, which is not a port, so that probe finds nothing.

The desktop entry is KDOS's own, with `Terminal=true`.
