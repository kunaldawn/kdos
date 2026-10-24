## About

[Cube 2: Sauerbraten]{.idx} is a fast first-person shooter on the Cube 2 engine, best known for
its in-game map editing, which several players can do together on one server. The release archive
carries the source, the complete game data and prebuilt engines; this port compiles the client
and the server from the source, installs the data, and never installs the prebuilt engines in
`bin_unix/`.

## Flags

- `CXXFLAGS` — with `CFLAGS`: the tree's flags followed by upstream's own optimisation flags,
  `-O3 -fomit-frame-pointer`, and `-ffast-math` for C++. The engine's makefile assigns
  `CXXFLAGS= -O3 -fomit-frame-pointer -ffast-math`, and the bundled ENet's makefile
  `CFLAGS=-O3 -fomit-frame-pointer`; an assignment in a makefile takes precedence over the
  environment, so the tree's flags reach the compiler only on the `make` line, which also passes
  them down to ENet's sub-make. Without them the file-prefix map is lost too, and the binary
  records the build path.
- `CLIENT_INCLUDES` — with `CLIENT_LIBS`: the client's include and library lines, given whole.
  Upstream's Linux lines call `sdl2-config`, which [sdl2-compat](#port-sdl2-compat) does not
  install, so SDL2's flags come from `pkg-config` instead. The rest of each line is upstream's:
  the bundled ENet, `libX11`, SDL2_image, SDL2_mixer, zlib, `libGL` and `librt`. `CLIENT_LIBS`
  begins with the tree's `LDFLAGS`, which the makefile's link line does not otherwise read.
- `SERVER_LIBS` — the server's library line, upstream's, with the tree's `LDFLAGS` in front for the
  same reason.

## Patches

- `placement-new.patch` — In `src/shared/tools.h`, replaces the engine's own inline definitions of placement `new`, `new[]`, `delete` and `delete[]` with `#include <new>`. libstdc++'s `<cmath>` brings in `<new>` before `tools.h` is read, and a second definition of those operators does not compile.

## Notes

The bundled ENet networking library, in `src/enet`, is built and linked statically by the
makefile's `libenet` target before the client and the server.

The engine reads its data from the working directory and writes the player's configuration, maps
and demos to the home directory given with `-q`. Upstream's `sauerbraten_unix` launcher does the
same, so `build.sh` installs the programs in `/usr/lib/sauerbraten`, the data in
`/usr/share/sauerbraten`, and `/usr/bin/sauerbraten` and `/usr/bin/sauerbraten-server` as small
scripts that change into the data directory and start the engine with
`-q$HOME/.sauerbraten`. The window's Wayland app id is SDL's default, the executable's name,
`sauer_client`, which is what `StartupWMClass` names.
