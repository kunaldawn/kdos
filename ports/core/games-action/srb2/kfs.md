## About

[Sonic Robo Blast 2]{.idx} is a 3D Sonic the Hedgehog fan game, built on a heavily modified Doom
Legacy engine, with single-player zones, multiplayer over the network and a large add-on
community. The engine is free software; the game data is not part of the source tree, so the port
takes it from upstream's full Windows release archive, a second source, and installs the data
files from it without its Windows program and libraries.

## Flags

- `-DSRB2_SDL2_EXE_NAME=srb2` — name the program `srb2`. Left empty, upstream builds the name from
  the git branch or commit (`lsdlsrb2` and a suffix on Linux), and a release tarball has no
  repository to ask.
- `-DSRB2_CONFIG_SYSTEM_LIBRARIES=ON` — use the system's libraries. In this release the variable is
  not declared as an option and is read in one place: with it on, the Game Music Emu headers come
  from the installed [libgme](#port-libgme); off, the build adds `${libgme_SOURCE_DIR}`, the
  directory of a bundled libgme source, which this release's `thirdparty/CMakeLists.txt` never
  sets up.
- `-DSRB2_CONFIG_STATIC_STDLIB=OFF` — do not link the C and C++ standard libraries statically.
  Upstream defaults it to `ON`, which needs every dependency static too; in this release the
  switch only takes effect in a MinGW build, so on Linux it is named for clarity.
- `-DSRB2_CONFIG_EXECINFO=OFF` — compile with `NOEXECINFO`, without the crash handler's stack-trace
  dump. That code uses `backtrace()` from `execinfo.h`, which musl does not have.
- `-DSRB2_CONFIG_HWRENDER=ON` — with `-DSRB2_CONFIG_STATIC_OPENGL=OFF`: build the OpenGL renderer
  beside the software one, looking up OpenGL's functions at run time rather than linking the
  program against the OpenGL library. Both are upstream's defaults, named.
- `-DSRB2_CONFIG_USE_GME=ON` — play the video-game chiptune formats through Game Music Emu, beside
  the Ogg music SDL_mixer plays. Upstream's default is `OFF`.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_libopenmpt=ON` — with the same switch for `libgme` and
  `miniupnpc`: make each of those three lookups required. Upstream looks for each one `QUIET` and
  builds without it when it is missing, losing tracker-module playback through
  [libopenmpt](#port-libopenmpt), the GME formats, or the automatic router port mapping of a hosted
  game through [miniupnpc](#port-miniupnpc), with nothing at configure time to say so.
- `-DCMAKE_DISABLE_FIND_PACKAGE_SDL2_mixer_ext=ON` — never look for SDL Mixer X, the extended fork
  of SDL_mixer that upstream prefers when it finds it. The tree has no port of it, and the build
  uses [sdl2-mixer](#port-sdl2-mixer).
- `-DSRB2_CONFIG_ERRORMODE=OFF` — with `-DSRB2_CONFIG_DEV_BUILD=OFF`: warnings are not errors, and
  the program is not a development build. Both are upstream's defaults, named.

## Notes

[curl](#port-curl) is a required dependency of this release: the game reaches the master server
through it and downloads the add-ons a network game uses.

The game data is `srb2-assets-<version>.zip`, upstream's full release archive. `build.sh` extracts
only the four `.pk3` archives (`srb2`, `zones`, `characters`, `music`), `models.dat`, the `models`
directory and the two licence files, and leaves out `srb2win.exe` and the Windows libraries. The
data goes into `/usr/share/games/SRB2`, which is on the program's own data search path, so no
setting is needed to find it. `unzip` is in `depends` for this step.

The build runs from a `build` directory inside the source: upstream's configure refuses a build in
the source directory itself. Only the program is installed from it, by hand; the icon is the
`srb2.png` in the source, and the window's Wayland app id is SDL's default, the executable's name,
`srb2`, which is what `StartupWMClass` names.
