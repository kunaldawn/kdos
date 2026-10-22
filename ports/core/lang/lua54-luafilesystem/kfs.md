## About

LuaFileSystem is the `lfs` module: `stat`, `mkdir`, `rmdir`, directory iteration, locking and
symbolic-link queries for Lua, which the language's own standard library leaves out. It is one C
file compiled into `lfs.so`, a module Lua loads with `require "lfs"`.

In KDOS it exists for [Prosody](#port-prosody), the XMPP server, which is its only reverse
dependency. Prosody runs on Lua 5.4, so this port builds against [lua54](#port-lua54), the 5.4
interpreter installed beside the newer one of the [lua](#port-lua) port, and installs into that
version's own module directory, `/usr/lib/lua/5.4`. Prosody's startup check,
`util/dependencies.lua`, treats a missing `lfs` as fatal and refuses to start. The port is built in
`30_foundation` with the other Lua 5.4 modules Prosody depends on,
[lua54-luasocket](#port-lua54-luasocket), [lua54-luasec](#port-lua54-luasec) and
[lua54-luaexpat](#port-lua54-luaexpat).

The project has no configure step. Its `Makefile` includes a file named `config` that sets the Lua
version, the include path, the compiler flags and the link command as `make` variables, and
`build.sh` overrides each of them on the command line instead of editing that file.

## Flags

- `LUA_VERSION` — the Lua version the module is built for. `config` defaults it to an older Lua. It names
  the install directory, `$(PREFIX)/lib/lua/$(LUA_VERSION)`, and the versioned include directories
  of `config`'s default `LUA_INC`; on both lines it is the recipe's `_lv` helper, `5.4`.
- `LUA_INC` — the compiler's include path for `lua.h`. The default adds `$(PREFIX)/include`, which
  is `/usr/local/include`, as well as the versioned directories; this sets only
  `/usr/include/lua5.4`, where the [lua54](#port-lua54) port installs its headers. `config` reads
  `LUA_INC` only inside its own `CFLAGS`, which the next switch replaces, so the include directory
  that reaches the compiler is the one written into that replacement.
- `CFLAGS` — replaces `config`'s whole `CFLAGS`, which is its warnings, `-O2 -fPIC` and the include
  path. The replacement is the tree's exported flags with `-fPIC` and the include directory
  appended. The tree's flags already carry `-fPIC`; the include directory has to be named here,
  because the override drops `config`'s.
- `LIB_OPTION` — the link command's options. `config` sets `-shared` and nothing else, so the module
  would be linked without the tree's `LDFLAGS` (`-z now`, `--as-needed`, the build ID); the
  replacement keeps `-shared` and adds them.
- `PREFIX=/usr` — the install root. `config` defaults to `/usr/local`, which the package lint
  refuses.

## Notes

The include directory in `CFLAGS` and `LUA_VERSION` on the install line must name the same Lua
version. The first chooses the headers and the second the directory, so with two different values
the module is compiled for one interpreter and put where only the other looks for it; the recipe
takes both from `_lv`. Lua 5.4 looks for C modules in `/usr/lib/lua/5.4` because the
[lua54](#port-lua54) port's `lua-root-usr.patch` changes `LUA_ROOT` in `luaconf.h` from
`/usr/local/` to `/usr/`; without that patch, a module installed here would not be found.
