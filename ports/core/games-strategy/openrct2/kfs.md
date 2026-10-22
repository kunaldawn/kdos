## About

OpenRCT2 is an open-source re-implementation of the RollerCoaster Tycoon 2 engine: the theme-park
game, with a larger window, multiplayer and plugin scripting. The port builds the engine and
installs the free asset sets the project publishes, its objects, title sequences, sound effects
and music. The original game's graphics and scenarios cannot be shipped, so the game needs the
player's own copy of RollerCoaster Tycoon 2 or RCT Classic, and asks where it is at first start.

## Flags

- `-DJSON_HAS_FILESYSTEM=0`, `-DJSON_HAS_EXPERIMENTAL_FILESYSTEM=0` — Compile out
  [nlohmann-json](#port-nlohmann-json)'s conversions between JSON and `std::filesystem::path`.
  OpenRCT2 compiles C++20 with `-fno-char8_t`, and in C++20 those conversions need `char8_t`, so
  the header does not compile in that build. The game converts no path through JSON.
- `-DDOWNLOAD_TITLE_SEQUENCES=OFF`, `-DDOWNLOAD_OBJECTS=OFF`, `-DDOWNLOAD_OPENSFX=OFF`,
  `-DDOWNLOAD_OPENMUSIC=OFF`, `-DDOWNLOAD_REPLAYS=OFF` — Switch off the install steps that
  download the asset sets and the test replays from GitHub. The four asset sets are sources of the
  recipe instead, unpacked into `data/` before the build (see Notes); the replays are only for the
  tests.
- `-DOPENRCT2_USE_CCACHE=OFF` — Do not compile through ccache, which upstream uses by default when
  it finds it.
- `-DWITH_TESTS=OFF`, `-DDISABLE_GOOGLE_BENCHMARK=ON` — Build neither the tests nor the benchmarks.
- `-DDISABLE_VERSION_CHECKER=ON` — No check for a newer release at start.
- `-DDISABLE_HTTP=ON` — Leaves out HTTP, and with it the public server list and the in-game
  downloads; libcurl is then not needed. Multiplayer over a direct address stays, as networking is
  a separate switch.
- `-DDISABLE_DISCORD_RPC=ON` — No Discord rich presence.
- `-DDISABLE_NETWORK=OFF`, `-DDISABLE_TTF=OFF`, `-DDISABLE_FLAC=OFF`, `-DDISABLE_VORBIS=OFF`,
  `-DDISABLE_OPENGL=OFF`, `-DENABLE_SCRIPTING=ON`, `-DPORTABLE=OFF`, `-DSTATIC=OFF` — Upstream's
  defaults, named: multiplayer (its packets are signed with [openssl](#port-openssl)), TrueType
  text through FreeType, FLAC and Ogg Vorbis audio, the OpenGL renderer, plugin scripting on the
  bundled QuickJS, and an ordinary dynamically linked install rather than a relocatable or static
  one.

## Patches

- `Werror.patch` — Removes `-Werror` from the compile flags in the top-level `CMakeLists.txt`. The flag is unconditional, so any warning this compiler adds fails the build (see [An upstream `-Werror`](../../../../docs/kdos/05-developer/build-troubleshooting.md#an-upstream--werror)).
- `gcc-includes.patch` — Adds `#include <cstdint>` to `src/openrct2/config/IniReader.hpp` and `src/openrct2-ui/UiContext.h`, and `#include <future>` to `src/openrct2/network/NetworkServerAdvertiser.cpp`. Each file uses those declarations without including their header, and the tree's libstdc++ does not provide them through the headers it does include (see [A standard header the source never includes](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-standard-header-the-source-never-includes)).

## Notes

The four asset archives are unpacked into the source tree before configure: the objects into
`data/object`, the title sequences into `data/sequence`, and the sound effects and music into
`data/`, where their `assetpack/` and `object/` directories join the rest. The install copies `data/` whole into
`/usr/share/openrct2`, so they arrive with the rest. Upstream pins each set by URL and SHA-256 in
`assets.json`; the recipe's `_objects`, `_titles`, `_sfx` and `_music` versions and their
`sha256` lines are the same pins, and a version bump of the engine has to carry that file's new
values into the recipe.

Upstream installs a scalable SVG icon beside the PNGs, and the recipe removes it: nothing in the
session rasterises an SVG icon (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).
The main desktop entry is replaced by one with `StartupWMClass=openrct2`, the Wayland app_id SDL
gives the window from the executable's name. Upstream's three hidden entries, for save files,
scenarios and `openrct2:` links, are kept as they are.
