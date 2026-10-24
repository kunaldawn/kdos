## About

Pioneer is an open-ended space game: trading, missions and combat across a procedurally generated
Milky Way, flown with Newtonian physics. The port installs the game, `pioneer`, its data under
`/usr/share/pioneer/data`, and three tools upstream builds beside it, the system editor, the model
compiler and the save-game dumper, as `pioneer-editor`, `pioneer-modelcompiler` and
`pioneer-savegamedump`. It renders through OpenGL with SDL2 for its window and OpenAL for sound.

## Flags

- `-DUSE_SYSTEM_LIBGLEW=ON` — Links the system [glew](#port-glew) in place of the copy in
  `contrib/glew`. Under Wayland `glewInit()` returns `GLEW_ERROR_NO_GLX_DISPLAY`, and the renderer
  accepts that answer when SDL's video driver is `wayland` or `KMSDRM`. The check is compiled only
  when the GLEW header defines that error, which the system one does and the bundled one does not,
  so with the bundled GLEW the renderer fails to start on a Wayland session.
- `-DCMAKE_IGNORE_PATH='/usr/lib/cmake/glew;/usr/lib64/cmake/glew;/lib/cmake/glew;/lib64/cmake/glew'` — Hides GLEW's own CMake package, so
  CMake's `FindGLEW` searches for the header and library itself. Upstream declares an empty
  `GLEW::GLEW` target before the search, and GLEW's `glew-config.cmake`, finding a target of that
  name it did not create, stops configure with an error. Even without that, `FindGLEW` taking a
  shared GLEW from the package reads its path only from the Windows import-library property and
  leaves `GLEW_LIBRARIES` empty. Both paths are named because `/lib` is a link to `usr/lib` (see
  [A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
- `-DUSE_SYSTEM_LIBLUA=OFF` — Compiles the Lua in `contrib/lua`, which is Lua 5.2, the version the
  game's scripts are written for. The tree's Lua ports are other versions.
- `-DUSE_SSE42=OFF` — Compiles without `-msse4.2 -mpopcnt`. Upstream turns the option on for every
  x86 target, and a processor without SSE4.2 then stops on the first such instruction rather than
  running more slowly.
- `-DWITH_DEVKEYS=OFF` — Leaves out the extra key bindings for developer functions, which are on
  by default.
- `-DBUILD_WITH_OPENAL=ON` — Requires [openal-soft](#port-openal-soft) for sound; named, a missing
  OpenAL stops configure instead of building a silent game.
- `-DUSE_AVX2=OFF`, `-DWITH_OBJECTVIEWER=ON`, `-DREMOTE_LUA_REPL=OFF`, `-DPROFILER_ENABLED=OFF` —
  Upstream's defaults, named: no AVX2 code, the in-game object viewer built, no Lua console on a
  TCP port, and no built-in profiler.
- `-DPIONEER_DATA_DIR=/usr/share/pioneer/data`, `-DPIONEER_INSTALL_DATADIR=share/pioneer`,
  `-DPIONEER_INSTALL_BINDIR=bin` — The path compiled into the game as the place its data is read
  from at run time, and where the install puts the data and the programs. These are the values
  `cmake/InstallPioneer.cmake` derives on Linux when they are not given.

## Patches

- `musl-select.patch` — Adds `#include <sys/select.h>` to the bundled `contrib/nanosockets/nanosockets.h`, which uses `select()` and `fd_set`. musl declares them only there; its socket headers do not include it.
- `cstdint.patch` — Adds `#include <cstdint>` to `src/DateTime.h`, which uses the fixed-width integer types and includes only `<string>`, which does not declare them with the tree's libstdc++ (see [A standard header the source never includes](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-standard-header-the-source-never-includes)).

## Notes

Upstream installs the three tools as `editor`, `modelcompiler` and `savegamedump`; the loop after
the install gives each a `pioneer-` prefix.

`ninja build-data` runs before the install and is not optional. It runs the model compiler just
built over `data/models`, writing each ship and station model in the optimised `.sgm` form beside
its source, and the install copies only `.sgm`, `.dds` and `.png` files from that directory. An
install without the step carries no models at all.

The game's strings are JSON files under `data/lang/<module>/`, one per language. Every one but
`en.json` is deleted after the install, because the image is English only (see
[Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy));
the package's language rule matches message catalogs and does not reach these files.

Upstream's desktop entry is replaced by one with `StartupWMClass=pioneer`, the Wayland app_id SDL
gives the window from the executable's name. The PNG icons it names are upstream's.
