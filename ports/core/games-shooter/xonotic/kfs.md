## About

[Xonotic]{.idx} is a fast arena first-person shooter on the DarkPlaces engine, an extended Quake
engine. This port builds the engine twice, as the SDL client `xonotic` and the dedicated server
`xonotic-dedicated`, together with d0_blind_id, the player-identification and encryption library
the engine uses for its player IDs. The game's maps, models, sounds and textures are the separate
[xonotic-data](#port-xonotic-data) port, installed in `/usr/share/xonotic`.

## Flags

- `CPUOPTIMIZATIONS` — set to the tree's `CFLAGS` and `LDFLAGS`, with `-O2` raised to `-O3` and
  `-std=gnu11` removed. The DarkPlaces makefile sets its own `CFLAGS` and `LDFLAGS` and ignores the
  exported ones; `CPUOPTIMIZATIONS` is the one variable it appends to its release compile line,
  and it comes last there, so its `-O2` would override the makefile's own `-O3`. The release link
  line repeats it ahead of the libraries, so the link flags travel in it too; on a compile-only
  line the compiler ignores the `-Wl,` options. `-std=gnu11` comes out because, with
  `gcc-15.patch` applied, `dpsoftrast.c` uses `bool` as the C23 keyword, which GNU C11 does not
  have (see [the release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags)).
- `SDL_CONFIG='pkg-config sdl2'` — the command the makefile runs for SDL2's flags. Its default is
  `sdl2-config`, which [sdl2-compat](#port-sdl2-compat) does not install.
- `SDLCONFIG_UNIXLIBS_X11=` — empty, so the SDL client does not link `libX11`. Built on SDL2 it
  calls no Xlib function (its X11 code is for SDL 1.2, and the GLX client is a separate target), but
  the makefile adds `-lX11` to the SDL client's link all the same.
- `DP_FS_BASEDIR=/usr/share/xonotic` — compile in the base directory where the engine looks for
  the game data, the directory [xonotic-data](#port-xonotic-data) installs.
- `sdl-release` — with `sv-release`: the two DarkPlaces targets built, the SDL client and the
  dedicated server, each with the makefile's release optimisation level.

## Patches

- `gcc-15.patch` — In DarkPlaces' `qtypes.h`, renames the members of the `qboolean` enum from `false` and `true` to `qfalse` and `qtrue`, with `#define false qfalse` and `#define true qtrue` after it, and in `dpsoftrast.c` removes `typedef qboolean bool;`. In C23, GCC's default language, `bool`, `true` and `false` are keywords, and the original lines do not compile.

## Notes

The source release is a `.zip`, which kpkg copies into the work directory as it is (see
[Sources, and what each one becomes](../../../../docs/kdos/05-developer/writing-ports.md#sources-and-what-each-one-becomes)),
so `build.sh` unpacks it itself with `unzip` and builds from the `Xonotic` directory inside.
d0_blind_id is built first, with its own `configure`, as a shared library: DarkPlaces opens it at
run time with `dlopen`, as it does libpng, libogg, libvorbis, libtheora and libcurl, so those are
in `depends` although no link line names them. Its big-number arithmetic is on
[gmp](#port-gmp).

DarkPlaces picks the game it runs from its executable's name, which is why the client is installed
as `xonotic`; that name is also SDL's default Wayland app id, which `StartupWMClass` names.
