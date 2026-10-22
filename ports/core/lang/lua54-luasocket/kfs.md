## About

LuaSocket gives Lua TCP, UDP and DNS, and on Unix also local sockets and serial lines, plus the
`mime` module and pure-Lua helpers for HTTP, FTP, SMTP and URLs. This port builds it for
[lua54](#port-lua54) alone: it is the network layer of [prosody](#port-prosody), the XMPP server,
and [lua54-luasec](#port-lua54-luasec) adds TLS on top of it. The C modules go to
`/usr/lib/lua/5.4/socket` and `/usr/lib/lua/5.4/mime`, the Lua ones to `/usr/share/lua/5.4`, the
two directories lua54's interpreter searches.

LuaSocket has no configure step. Its makefile defaults to Lua 5.1 and a `/usr/local` prefix, and
every setting is a `make` variable, so the recipe passes them all on the command line. A
`make VAR=value` argument overrides even a plain assignment in the makefile, such as
`CFLAGS_linux`; an exported variable overrides only the `?=` defaults.

## Flags

- `PLAT=linux` — Selects the makefile's Linux settings, which is also its default.
- `LUAV=$_lv` — The Lua series to build for, `5.4`. It names the install directories
  `lib/lua/5.4` and `share/lua/5.4`.
- `LUAPREFIX_linux=/usr` — The install prefix on Linux, in place of upstream's `/usr/local`. It is
  passed to the install step as well.
- `LUAINC_linux_base=/usr/include` — Where the Lua headers are looked for, without the version:
  the makefile appends `/lua$(LUAV)` itself. It takes effect only through upstream's own
  `CFLAGS_linux`, which the next switch replaces, so the include directory actually used is the
  one that switch names.
- `CFLAGS_linux="$CFLAGS -I/usr/include/lua$_lv -DLUASOCKET_NODEBUG"` — Replaces upstream's
  compile line for Linux whole. Upstream's line comes after anything `MYCFLAGS` adds and carries its
  own `-O2 -ggdb3`, so flags passed that way could not override it. The replacement carries the tree's `CFLAGS`, lua54's
  header directory and `LUASOCKET_NODEBUG`, the define upstream's line sets by default. The debug
  build it rules out adds a `_DEBUG` field to the module and an elapsed-time value to what each
  send and receive returns. Upstream's `-fpic` is not repeated, because the
  tree's `CFLAGS` carry `-fPIC`.
- `MYLDFLAGS="$LDFLAGS"` — Passes the tree's linker flags; the makefile puts them after the
  objects and ahead of its own `-O -shared -fpic -o`.
- `install-unix` — Installs everything `install` does, and also the Unix-only `socket.unix` and
  `socket.serial` modules.
- `prefix=/usr` — The install prefix, which the makefile otherwise derives from
  `LUAPREFIX_linux`.
