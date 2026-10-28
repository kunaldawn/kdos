## About

Lua is a small embeddable scripting language. This port is the system Lua: the `lua` interpreter,
`luac`, and `liblua.so` with its headers directly in `/usr/include`. On the desktop's own path,
[wireplumber](#port-wireplumber), PipeWire's session manager, runs its policy scripts in it (see
[Audio](../../../../docs/kdos/03-architecture/session.md#audio));
wireshark, weechat, gnuplot, sc-im, tio and celestia embed it too.

Software that cannot move to this series uses [lua54](#port-lua54), installed beside it under
versioned names.

Upstream's makefile builds only a static library and installs no pkg-config file. The recipe
links a shared library from that archive itself and writes `lua.pc`.

## Flags

- `CC=cc` — Replaces upstream's `CC= gcc -std=gnu99`. The language standard then comes from the
  tree's `CFLAGS`, which name `-std=gnu11`.
- `MYCFLAGS="$CFLAGS -DLUA_COMPAT_5_2 -DLUA_COMPAT_5_1 -fPIC"` — `MYCFLAGS` is the variable
  upstream's makefile leaves for the builder; its own `CFLAGS` is assigned in the makefile, so the
  tree's flags reach the compiler only through here. `-fPIC` is needed because the shared library
  is linked from the objects of `liblua.a`; the tree's `CFLAGS` carry it as well. The two
  `LUA_COMPAT_` macros are tested by none of this release's sources, whose compatibility switches
  are `LUA_COMPAT_GLOBAL`, `LUA_COMPAT_MATHLIB` and `LUA_COMPAT_APIINTCASTS`, so they change
  nothing in the build.
- `MYLDFLAGS="$LDFLAGS"` — Passes the tree's linker flags into the link lines of `lua` and
  `luac`.
- `linux-readline` — Builds for Linux with `LUA_USE_LINUX` and `LUA_USE_READLINE`, linking
  readline into the `lua` interpreter for line editing and history at its prompt. `liblua.so` is
  linked from `liblua.a` and does not link readline.
- `-Wl,-soname,liblua.so.${_majorver}` — Gives the shared library the soname
  `liblua.so.<series>`, so a program linked against it records the series, not the full
  version.
- `-Wl,-whole-archive` — Puts every object of `liblua.a` into the shared library, not only the ones
  something references.
- `INSTALL_TOP=$PKG/usr` — Installs under `/usr` in the package, rather than upstream's
  `/usr/local`.
- `TO_LIB="liblua.so liblua.so.$_majorver liblua.so.$version"` — Has `make install` install the
  shared library and its two links in place of `liblua.a`, so the static archive is not installed.
- `INSTALL_DATA="cp -d"` — Copies the two links as links; upstream's `install -m 0644` would copy
  the library three times.
- `INSTALL_MAN=$PKG/usr/share/man/man1` — Puts `lua.1` and `luac.1` where `man` looks, rather
  than upstream's `$(INSTALL_TOP)/man/man1`.

## Patches

- `lua-root-usr.patch` — Changes `LUA_ROOT` in `src/luaconf.h` from `/usr/local/` to `/usr/`, so the default `package.path` and `package.cpath` search `/usr/share/lua/<version>` and `/usr/lib/lua/<version>`, where this tree installs Lua modules.

## Notes

`lua.pc` is written by two here-documents, and the split matters. The first is unquoted and
expands only `V` and `R`, the version series and the full version. The second is quoted, so
every `${prefix}`, `${libdir}` and `${V}` in it reaches the file literally, as the pkg-config
variables they are. Written as one quoted here-document, the `V=` and `R=` lines would hold the
literal text `$_majorver` and `$version`, `Version:` would expand to `$version`, and every consumer that asks for a minimum version would be told Lua is absent: tio, for one,
asks meson for `lua` with `version: '>=5.1'`.

`lua$_majorver.pc` is a link to `lua.pc`. lua54 installs `lua5.4.pc`, and a consumer that names
its Lua by version finds this one only through the link: wireplumber is built with
`-Dsystem-lua-version` set to this series and asks pkg-config for `lua-X.Y`, then `luaX.Y`. A
consumer that tries versioned names before plain `lua` would otherwise settle on 5.4.

Lua's headers in `/usr/include` are this series'. A program that bundles its own Lua, or a 5.4
module, must put its own include directory first; see
[A bundled header shadowed by the system's](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-bundled-header-shadowed-by-the-systems).
