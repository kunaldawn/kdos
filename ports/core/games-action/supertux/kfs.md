## About

[SuperTux]{.idx} is a side-scrolling platform game in the style of the classic console
platformers, with Tux as the hero, a world map, and a level editor built into the game. It draws
through SDL2 and OpenGL and plays its music and sound through OpenAL; the program is
`supertux2`, and its levels, graphics and music install under `/usr/share/games/supertux2`.

## Flags

- `-DINSTALL_SUBDIR_BIN=bin` — install `supertux2` into `/usr/bin`. On Linux upstream's default is
  `games`, which would put it in `/usr/games`.
- `-DIS_SUPERTUX_RELEASE=ON` — build as an official release: the title screen shows the release
  logo rather than the development one, the window takes the release icon, and the installed
  icon is `supertux2` rather than `supertux-nightly`. Upstream's default is `ON` too unless the
  source is a git checkout that is not on a tag; naming it keeps the result independent of how the
  source arrived. It also turns precompiled headers on by default, which `-DSUPERTUX_PCH=OFF`
  overrides.
- `-DENABLE_OPENGL=ON` — with `-DUSE_GL_LIBRARY=glew`: draw through OpenGL, loading its functions
  with [glew](#port-glew) (the alternative is libepoxy). Without OpenGL the game falls back to its
  SDL renderer. Both are upstream's defaults, named.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_OpenGL=ON` — make the `find_package(OpenGL)` call required.
  Upstream asks for OpenGL quietly and builds without the OpenGL renderer when it is not found, so
  a build root missing [libglvnd](#port-libglvnd) would otherwise give a game with only the SDL
  renderer and no error.
- `-DENABLE_NETWORKING=OFF` — build without libcurl. Networking in SuperTux is the add-on
  downloader, which fetches add-ons and their index over HTTP; without it the add-on menu cannot
  download, and [curl](#port-curl) is not in `depends`.
- `-DENABLE_DISCORD=OFF` — with `-DSTEAM_BUILD=OFF` and `-DFLATPAK=OFF`: no Discord rich-presence
  client, and none of the Steam or Flatpak variations (a Flatpak build, for one, forces SDL's
  Wayland driver and drops the script menu's "Open in editor" entry). The Discord client's source
  is in `external/discord-sdk`, but its CMake file downloads RapidJSON with `FetchContent` at
  configure time, which an offline build cannot do. All three are off by default, named.
- `-DUSE_SYSTEM_SDL2_TTF=ON` — link the tree's [sdl2-ttf](#port-sdl2-ttf) instead of compiling the
  copy of SDL_ttf in `external/`. Upstream's bundled copy can shape text with libraqm; the system
  library does not, and upstream warns at configure time that right-to-left scripts such as Arabic
  are then not displayed properly.
- `-DUSE_STATIC_SIMPLESQUIRREL=ON` — link the SimpleSquirrel scripting library (SuperTux's wrapper
  around the Squirrel language its levels are scripted in) statically into `supertux2`. With it
  `OFF`, upstream's default on Linux, the build installs a separate `libsimplesquirrel` shared
  library that only this program uses.
- `-DSUPERTUX_PCH=OFF` — compile without precompiled headers, which `IS_SUPERTUX_RELEASE` would
  otherwise switch on. Precompiled headers only shorten the compile.
- `-DSUPERTUX_LTO=OFF` — no link-time optimisation, upstream's default, named.
- `-DCMAKE_DISABLE_FIND_PACKAGE_Git=ON` — keep CMake from looking for `git`. Upstream reads the
  version from git only when the source has a `.git` directory, which an unpacked release does
  not; the version then comes from the release's own `version.cmake` rather than from
  `git describe`.

## Patches

- `musl-execinfo.patch` — In `src/supertux/error_handler.cpp`, includes `<execinfo.h>` only when `__has_include` finds it, and compiles the Unix stack-trace code (`backtrace()` and `backtrace_symbols()`) only then. musl has no `execinfo.h`, and the tree carries no `libexecinfo`, so unpatched the file does not compile; patched, the crash handler on KDOS reports the error without a call stack.

## Notes

`build.sh` deletes every `.po` file under `/usr/share/games/supertux2`. SuperTux reads its
translations as `.po` files at run time, through its bundled tinygettext, and kpkg's
[language prune](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)
removes compiled catalogues (`.mo`, `.gmo`, `.qm`) but not `.po` files, so without the deletion the
game would ship every translation while the rest of the system is English only.

Upstream installs `supertux2.svg` as its hicolor icon. The desktop's icon lookup reads PNGs only
(see [Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)),
so `build.sh` renders a 256-pixel PNG from it with `rsvg-convert` from [librsvg](#port-librsvg),
and writes its own `supertux2.desktop` over upstream's, untranslated, with search keywords.
