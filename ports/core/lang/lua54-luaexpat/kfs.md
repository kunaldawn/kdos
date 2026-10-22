## About

LuaExpat is the [expat](#port-expat) XML parser for Lua: its `lxp` module calls Lua functions for
each element as the parser reads, and `lxp.lom` and `lxp.totable` build tables from a document.
This port builds it for [lua54](#port-lua54) alone. XMPP is XML on the wire, and
[prosody](#port-prosody) parses every stream through it; Prosody's dependency check counts a
missing `lxp` as fatal, and the server does not start without it.

Every value the recipe passes is also the makefile's own default in this release.

## Flags

- `CFLAGS="$CFLAGS -g0"` — The makefile's own flags put `-g` before `$(CFLAGS)`; `-g0`, last on the
  line, turns the debugging information back off.
- `LUA_V=$_lv` — The Lua series, `5.4`. It is passed, like every path, on the `make` command
  line, where it beats the makefile's `?=` default.
- `LUA_INC="-I/usr/include/lua$_lv"` — The Lua header directory, with its own `-I`. The makefile
  pastes this variable straight into the compile line, so a bare path there would be read by the
  compiler as a source file.
- `EXPAT_INC="-I/usr/include"` — expat's header directory, under the same rule.
- `LUA_LDIR=/usr/share/lua/$_lv` — Where the Lua parts, `lxp/lom.lua`, `lxp/totable.lua` and
  `lxp/threat.lua`, go. It is passed to the install step as well.
- `LUA_CDIR=/usr/lib/lua/$_lv` — Where `lxp.so` goes.
