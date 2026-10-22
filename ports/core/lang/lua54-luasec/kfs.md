## About

LuaSec is the TLS layer for [LuaSocket](#port-lua54-luasocket), built on
[openssl](#port-openssl): it wraps a socket in an encrypted connection and adds `ssl.https`. This
port builds it for [lua54](#port-lua54) alone, for [prosody](#port-prosody), whose TLS it
provides. Prosody starts without LuaSec and reports it as a missing dependency, but by default it
requires an encrypted stream before a client may authenticate (its `c2s_require_encryption`
option), so without LuaSec no client can log in.

LuaSec has no configure step, and its makefile defaults to Lua 5.1's module directories, so the
recipe passes every path on the `make` command line, where it beats the makefile's own `?=`
defaults.

## Flags

- `linux` — Builds with upstream's Linux compile and link flags. The bare `make` builds nothing and
  prints the list of platforms.
- `LUA_VERSION=$_lv` — Read by none of LuaSec's makefiles; it changes nothing in the build. What
  ties the build to 5.4 is the include path and the two install directories.
- `INC_PATH="-I/usr/include/lua$_lv"` — The header directory, with its `-I`, in place of
  upstream's `-I/usr/include`. The lua port's headers are in `/usr/include`, so the default would
  compile a 5.4 module against the newer series' `lua.h`.
- `LUAPATH=/usr/share/lua/$_lv` — Where `ssl.lua` and `ssl/https.lua` go, in place of upstream's
  `/usr/share/lua/5.1`. It is passed to the install step as well.
- `LUACPATH=/usr/lib/lua/$_lv` — Where `ssl.so` goes, in place of `/usr/lib/lua/5.1`.

## Notes

LuaSec compiles its own private copy of a few LuaSocket C files into `ssl.so` and links that, not
the installed LuaSocket. The dependency on [lua54-luasocket](#port-lua54-luasocket) is for run
time: `ssl.https` and Prosody load the `socket` module it wraps.

The tree's `CFLAGS` and `LDFLAGS` reach the build from the environment, because LuaSec's makefile
adds its own flags to them with `+=` rather than assigning them.
