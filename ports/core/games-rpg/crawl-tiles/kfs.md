## About

[Dungeon Crawl Stone Soup]{.idx} is a single-player roguelike: the player descends a dungeon of
randomly generated levels in search of the Orb of Zot, with permanent death and a large choice of
species, backgrounds and gods. This port is its graphical tiles build, the program `crawl`,
drawn with SDL2 and OpenGL and with sound; the game's level and monster data install under
`/usr/share/crawl`, and saves go to `~/.crawl`.

## Flags

- `TILES=y` — with `SOUND=y`: the graphical tiles build rather than the console one, with sound
  through [sdl2-mixer](#port-sdl2-mixer).
- `EXTERNAL_FLAGS` — with `EXTERNAL_LDFLAGS`: hand the tree's `CXXFLAGS` and `LDFLAGS` to the
  build. The Makefile assigns `CFLAGS` and `LDFLAGS` itself, so values from the environment never
  reach the compiler; these two variables are the ones it appends.
- `bin_prefix=bin` — install `crawl` into `/usr/bin`. With `prefix=/usr` the Makefile's default is
  `games`, which would put it in `/usr/games`.
- `NO_TRY_GOLD=y` — with `NO_TRY_LLD=y`: link with the compiler's default linker. Without them the
  Makefile links a test program with `-fuse-ld=lld`, then with `-fuse-ld=gold`, and uses the first
  that works, so the linker would depend on what the build root holds.
- `LANGUAGES=` — install no translated description files. The Makefile's default is every language
  directory under `dat/descript` except English; the game is English only, like the rest of the
  system.
- `PROPORTIONAL_FONT=/usr/share/fonts/TTF/DejaVuSans.ttf` — with
  `MONOSPACED_FONT=/usr/share/fonts/TTF/DejaVuSansMono.ttf`: compile in the paths of the DejaVu
  faces that [ttf-dejavu](#port-ttf-dejavu) installs. Left unset, the Makefile searches the build
  root for those faces and, when it does not find them, installs the copies under
  `contrib/fonts` into the game's data directory.

## Notes

The `mk` function passes the same variables to both `make` runs: upstream's Makefile reads the
build type at install time too, so `make install` without `TILES` and the rest would install a
different build.

Every library is the system's. In a native Linux build the Makefile finds SDL2, FreeType, SQLite,
zlib, libpng and Lua through `pkg-config`, so nothing under `contrib/` is compiled. Lua is
[lua54](#port-lua54), found as `lua5.4`. The Makefile falls back without a word: when no Lua 5.4
module answers `pkg-config`, it compiles and links the copy under `contrib/lua` instead, and the
build still succeeds. [perl](#port-perl), [python3](#port-python3) and
[python3-yaml](#port-python3-yaml) are in `depends` for the build-time generators, such as
`util/mon-gen.py`, which writes the monster table from `dat/mons/*.yaml`.

`build.sh` installs upstream's tile icons into hicolor and writes the desktop entry, with
`StartupWMClass=crawl`: the window's Wayland app id is SDL's default, the executable's name.
