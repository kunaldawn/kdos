## About

mGBA emulates the Game Boy Advance, Game Boy and Game Boy Color. This port builds `libmgba` and
the SDL front end, `mgba`, which runs one ROM named on its command line; the libretro core is the
[libretro-mgba](#port-libretro-mgba) port, built from the same tarball, and RetroArch is the menu's
emulator front end. The player supplies the ROMs; the replacement GBA BIOS that mGBA compiles in
is one of the exemptions listed in
[What is built from source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).

## Flags

- `-DBUILD_SHARED=ON`, `-DBUILD_STATIC=OFF` — Build `libmgba` as a shared library only. Both are
  upstream's defaults.
- `-DBUILD_SDL=ON` — The SDL front end, upstream's default.
- `-DBUILD_QT=OFF` — No Qt front end, which upstream builds by default.
- `-DBUILD_LIBRETRO=OFF` — No libretro core here; libretro-mgba builds it.
- `-DBUILD_TEST=OFF`, `-DBUILD_SUITE=OFF` — No test harness or test suite, as upstream's defaults.
- `-DBUILD_GL=ON` — This switch, `-DBUILD_GLES2=ON` and `-DBUILD_GLES3=ON` build the OpenGL, GLES 2
  and GLES 3 renderers, upstream's defaults. Upstream turns each off without an error when its
  library is not found.
- `-DUSE_EPOXY=ON` — Load OpenGL through [libepoxy](#port-libepoxy). It is upstream's default.
- `-DUSE_FFMPEG=OFF` — No FFmpeg. Its only use is the recording path of the Qt front end, which is
  not built.
- `-DUSE_LUA=OFF`, `-DENABLE_SCRIPTING=OFF` — No Lua scripting: the SDL front end has no way to
  load a script.
- `-DUSE_DISCORD_RPC=OFF` — No Discord presence.
- `-DUSE_ZLIB=ON` — This switch and `-DUSE_PNG=ON`, `-DUSE_LIBZIP=ON`, `-DUSE_LZMA=ON`,
  `-DUSE_SQLITE3=ON`, `-DUSE_ELF=ON` and `-DUSE_EDITLINE=ON` are upstream's defaults: compressed
  and zipped ROMs through [zlib](#port-zlib) and [libzip](#port-libzip), 7-Zip archives through
  the LZMA code in the source, PNG screenshots, the game database in [sqlite](#port-sqlite),
  homebrew ELF files through [elfutils](#port-elfutils)' libelf, and the command-line debugger
  through [libedit](#port-libedit).
- `-DUSE_MINIZIP=OFF` — Read zip archives through libzip, not minizip. Upstream uses only one of
  the two, libzip first.
- `-DUSE_GDB_STUB=ON` — The GDB remote stub, for debugging a game's ARM code with gdb. It is
  upstream's default.
- `-DSDL_VERSION=2` — Build the front end against SDL 2, here [sdl2-compat](#port-sdl2-compat).
  It is upstream's default; the alternative is SDL 1.2.

## Notes

The SDL front end needs a ROM on its command line, so its entry has `NoDisplay=true` and is an
open-with handler for the three ROM types rather than a menu row. SDL names the Wayland window
after the executable, `mgba`. The icon is upstream's, installed as `io.mgba.mGBA`.

`group = mgba` keeps this port and libretro-mgba one version bump: both build the same
tarball (see
[Steering the checker from a recipe](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#steering-the-checker-from-a-recipe)).
