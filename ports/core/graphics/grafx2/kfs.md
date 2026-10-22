## About

GrafX2 is a pixel-art paint program in the tradition of Deluxe Paint and Brilliance: indexed
colour, palette tools, brushes, animation frames and Lua scripts. It draws its own interface
through SDL, so it links no toolkit, and it loads many retro computer picture formats.

## Flags

- `API=sdl2` — Build the SDL 2 front end, which runs on [sdl2-compat](#port-sdl2-compat) over
  SDL 3. The makefile's default is SDL 1.2.
- `NO_X11=1` — Compile out the X11 clipboard and window calls (`-DNO_X11`), which SDL 3 underneath
  never reaches with no X11 video driver.
- `LUAPKG=lua5.4` — Take the Lua flags from `lua5.4.pc`, the [lua54](#port-lua54) port. The
  makefile's own search tries plain `lua` first, which here is the [lua](#port-lua) port at a newer
  language version than GrafX2's scripts are written for.
- `CFLAGS="$CFLAGS -g0"` — The makefile's compiler line always carries `-g`, and the tree's flags
  come after it, so `-g0` builds the program without debug information.

## Notes

RECOIL, the retro-format picture decoder, and a 6502 emulator core are
compiled into the program from `3rdparty/`. When their sources are missing, `src/Makefile` calls
`3rdparty/Makefile`, which downloads them, and which asks GitHub and other upstream sites for
their newest library releases as soon as `make` reads it. The two later sources are those trees: `build.sh` copies them into `3rdparty/` first,
and applies `3rdparty/6502-illegal-opcode.patch`, which ships in the GrafX2 tarball, to the 6502
core, so the build fetches nothing. RECOIL's version, `_recoil` in the recipe, must be the
`RECOILVER` that `3rdparty/Makefile` names, or the makefile looks for a directory that is not
there.

[sdl2-compat](#port-sdl2-compat) installs no `sdl2-config`, and `src/Makefile` runs that name for
the SDL 2 flags. A two-line shim in `$SRC_ROOT/shim`, put first on `PATH`, answers from
`pkg-config sdl2` instead.

The makefile names the binary after its front end, `grafx2-sdl2`. It is installed as plain
`grafx2`, the name the window's app id carries; the program finds its data at `../share/grafx2`
from the binary, which the rename keeps.

Upstream ships its icon as SVG and XPM, which the panel does not read, so `build.sh` rasterises
the SVG into hicolor PNGs. Upstream's desktop entry is replaced: it claims PNG, GIF and BMP among
its MIME types, which belong to the image viewer.
