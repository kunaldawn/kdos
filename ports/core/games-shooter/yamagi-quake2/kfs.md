## About

[Yamagi Quake II]{.idx} is a Quake II engine that keeps the original game's look and play while
running on current systems, with OpenGL 1.4, OpenGL 3.2, OpenGL ES 3 and software renderers. It
installs the client `quake2` and the dedicated server `q2ded`. The game data is not free and is
not shipped: a player copies `baseq2/pak0.pak` and the rest of their own Quake II into
`/usr/share/games/quake2/baseq2`, the directory the desktop entry's comment names.

## Flags

- `-D_LARGEFILE64_SOURCE` — appended to `CFLAGS`. The engine's copy of minizip, in
  `src/common/unzip`, calls `fopen64`, `ftello64` and `fseeko64` on every platform but macOS and
  Haiku; musl declares those names only when `_LARGEFILE64_SOURCE` is defined.
- `WITH_SDL3=yes` — with `WITH_OPENAL=yes`: build on [sdl3](#port-sdl3), and with the OpenAL sound
  backend, which the engine loads at run time (its `al_driver` setting names the
  library) from [openal-soft](#port-openal-soft). Both are upstream's defaults, named.
- `WITH_CURL=yes` — HTTP downloads of the files a server's game needs, through cURL, which the
  engine also loads at run time; [curl](#port-curl) is in `depends` for it. Upstream's default,
  named.
- `WITH_EXECINFO=no` — no crash backtrace. Upstream's default is `yes`, and the code uses
  `execinfo.h`, which musl does not have.
- `WITH_RPATH=no` — no `$ORIGIN/lib` run path, which upstream sets by default so that libraries
  placed in a `lib` directory beside the program, such as a patched SDL, are loaded first.
- `WITH_XDG=yes` — keep the user's configuration and saves under the XDG base directories.
  Upstream's default, named.
- `WITH_SYSTEMWIDE=yes` — with `WITH_SYSTEMDIR=/usr/share/games/quake2`: also search a
  system-wide directory for game data and `game.so`, and compile that directory in. Upstream's
  default is no system-wide directory at all; `/usr/share/games/quake2` is the directory the engine
  falls back to when `SYSTEMWIDE` is on and no `SYSTEMDIR` is given, so `WITH_SYSTEMDIR` names it.

## Notes

The Makefile has no install target, so `build.sh` installs by hand. The engine loads its renderer
libraries (`ref_gl1.so`, `ref_gl3.so`, `ref_gles3.so`, `ref_soft.so`) from the directory its
executable is in and from nowhere else, and finds `baseq2/game.so` through the same search path as
the game data, which includes that directory. So the whole `release/` tree is kept together in
`/usr/lib/yamagi-quake2`, and `/usr/bin` holds symbolic links to `quake2` and `q2ded`. Upstream's `yq2.cfg` goes into the system-wide `baseq2`.

The window's Wayland app id is the executable's name, `quake2`, which is what the entry's
`StartupWMClass` names.
