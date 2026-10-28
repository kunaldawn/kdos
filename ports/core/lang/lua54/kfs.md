## About

Lua 5.4, installed beside the system [lua](#port-lua) rather than in place of it. It exists for
software that runs on 5.4 and not on the newer series: [prosody](#port-prosody), the XMPP server,
together with the four modules it needs, [lua54-luasocket](#port-lua54-luasocket),
[lua54-luasec](#port-lua54-luasec), [lua54-luaexpat](#port-lua54-luaexpat) and
[lua54-luafilesystem](#port-lua54-luafilesystem). libinput, vlc, darktable, fastfetch, lite-xl,
grafx2 and crawl-tiles build against it as well. The recipe's `series = 5.4` keeps the version checker from offering a release past 5.4.

The two installs are kept apart by name. Every path this one writes carries the version: the
programs are `lua5.4` and `luac5.4`, the headers are in `/usr/include/lua5.4`, the libraries are
`liblua5.4.so` and `liblua5.4.a`, the pkg-config file is `lua5.4.pc`, and the manual pages are
`lua5.4.1` and `luac5.4.1`. Nothing here writes a path the lua port writes, so kpkg sees no
conflict and neither package shadows the other; see
[A second version beside the first](../../../../docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).

## Flags

- `CC=cc` — Replaces upstream's `CC= gcc -std=gnu99`, so the language standard comes from the
  tree's `CFLAGS`, which name `-std=gnu11`.
- `MYCFLAGS="$CFLAGS -DLUA_COMPAT_5_3 -fPIC"` — `MYCFLAGS` is how the tree's flags reach upstream's
  makefile, which assigns its own `CFLAGS`. `LUA_COMPAT_5_3` turns on the 5.3 compatibility set in
  `luaconf.h`: the deprecated mathematical functions, the integer-cast API macros such as
  `luaL_checkint`, the emulation of `__le` through `__lt`, and the old API names such as
  `lua_strlen`. Upstream's own `CFLAGS` line defines it too, so here it restates the default. `-fPIC` is needed because the shared library is linked from the objects of
  `liblua.a`; the tree's `CFLAGS` carry it as well.
- `MYLDFLAGS="$LDFLAGS"` — Passes the tree's linker flags into the link lines of `lua` and `luac`.
- `linux-readline` — Builds for Linux with readline linked into the interpreter, `lua5.4`, for line
  editing and history at its prompt; `linux` is an alias of `linux-noreadline`, the build without
  it. `liblua5.4.so` is
  linked from `liblua.a` and does not link readline.
- `-Wl,-soname,liblua${_majorver}.so.${_majorver}` — Gives the shared library the soname
  `liblua5.4.so.5.4`, distinct from the lua port's `liblua.so.<series>`.
- `-Wl,-whole-archive` — Puts every object of `liblua.a` into the shared library, not only the ones
  something references.

## Patches

- `lua-root-usr.patch` — Changes `LUA_ROOT` in `src/luaconf.h` from `/usr/local/` to `/usr/`, so the default `package.path` and `package.cpath` search `/usr/share/lua/5.4` and `/usr/lib/lua/5.4`, where the `lua54-*` modules install.

## Notes

Upstream's `make install` is not used: it would write `lua`, `luac` and unversioned headers, the
lua port's own paths. Each file is installed by hand under its versioned name instead, and the
module directories `/usr/lib/lua/5.4` and `/usr/share/lua/5.4` are created empty for the
modules to fill.

`lua5.4.pc` is written by two here-documents, for the reason the [lua](#port-lua) port gives: the
first expands only `V` and `R`, and the second, quoted, keeps every `${prefix}`, `${libdir}` and
`${V}` for pkg-config to expand. Its `Cflags` name `/usr/include/lua5.4` and its `Libs` name
`-llua5.4`. A consumer that takes the headers from `/usr/include` instead gets the lua port's
newer ones; see
[A bundled header shadowed by the system's](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-bundled-header-shadowed-by-the-systems).
