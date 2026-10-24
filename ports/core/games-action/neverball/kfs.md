## About

[Neverball]{.idx} is a 3D game in which the player tilts the floor to roll a ball through an
obstacle course, collecting coins against the clock. Neverputt, a hot-seat minigolf game for up
to four players, is built from the same source on the same engine, and `mapc` compiles the levels
of both. All three draw with OpenGL through SDL2.

## Flags

- `-fcommon` — appended to the build's `CFLAGS`: let a global that is defined in more than one
  object file link as one common symbol. A header in the source defines a global without
  `extern`, so every file including it defines the variable again; GCC's default, `-fno-common`,
  makes that a multiple-definition error at link time. The makefile sets `CFLAGS := -O2` itself,
  so the build's own flags reach the compiler only by being passed on the `make` line.
- `SDL_CPPFLAGS` — with `SDL_LIBS`, `PNG_CPPFLAGS` and `PNG_LIBS`: the makefile asks `sdl2-config`
  and `libpng-config` for these, and the tree's SDL2 is [sdl2-compat](#port-sdl2-compat), which
  installs no `sdl2-config`. All four are answered from `pkg-config` instead.
- `OGL_LIBS=-lOpenGL` — link `libOpenGL`, the vendor-neutral OpenGL library from
  [libglvnd](#port-libglvnd), rather than the makefile's `-lGL`, which pulls in GLX as well. The
  context comes from SDL, through EGL on Wayland, so GLX is not needed.
- `ENABLE_NLS=0` — build without gettext, so the games are English only and the `locales` target
  has nothing to do.
- `DATADIR=/usr/share/neverball` — compile in where the games find their data. The makefile's
  default is `./data`, the source tree, which only works for a game run from where it was built.

## Notes

Only the programs and the compiled levels are built: `make neverball neverputt mapc`, then
`make sols`, which runs `mapc` over every `.map` level source in `data/` and writes the compiled
`.sol` beside it. `build.sh` then copies `data/` whole and deletes the `.map` files, which nothing
reads once the levels are compiled. `mapc` is installed as `neverball-mapc`, with its manual page
renamed to match. The makefile's `desktops` target, which translates upstream's desktop entries,
is not run; `build.sh` writes both entries itself, and each window's Wayland app id is SDL's
default, the executable's name, which is what `StartupWMClass` names.
