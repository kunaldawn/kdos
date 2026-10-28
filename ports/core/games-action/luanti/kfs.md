## About

[Luanti]{.idx} is a voxel game engine: a world of blocks that can be mined and built with, where
the game itself is written in Lua and loaded at run time. The engine installs no playable game
(its source carries only Development Test, a test game), so this port also installs Minetest Game,
a sandbox game from the same project, written in Lua and taken from a second source, giving the
program something to play from its first start without downloading anything. The client renders
through IrrlichtMt, Luanti's own fork of the Irrlicht engine, on SDL3.

## Flags

- `-DBUILD_CLIENT=ON` — with `-DBUILD_SERVER=OFF`: build the `luanti` program, which plays local
  worlds and joins servers, and not the separate dedicated server `luantiserver`. Both are
  upstream's defaults, named.
- `-DBUILD_UNITTESTS=OFF` — leave out the unit tests, which upstream compiles into the program by
  default.
- `-DBUILD_DOCUMENTATION=OFF` — skip the Doxygen documentation, which upstream builds by default.
- `-DBUILD_BENCHMARKS=OFF` — with `-DBUILD_WITH_TRACY=OFF`: no benchmarks and no Tracy profiler
  client. Tracy is fetched from git at configure time, which a build without network cannot do.
  Both are upstream's defaults, named.
- `-DRUN_IN_PLACE=OFF` — a system install: the program reads its data from `/usr/share/luanti` and
  keeps worlds and settings in the user's home, rather than everything in the directory it runs
  from. Upstream's default on Linux, named.
- `-DENABLE_LTO=OFF` — no link-time optimisation. Upstream turns it on by default for a release
  build on Linux.
- `-DUSE_SDL3=ON` — build IrrlichtMt's SDL3 device rather than its default SDL2 one, linking
  [sdl3](#port-sdl3) directly. Upstream's build documentation calls the SDL3 device experimental.
- `-DENABLE_CURL=OFF` — with `-DENABLE_UPDATE_CHECKER=OFF`: build without libcurl, which upstream
  needs for the public server list, the in-game content browser (ContentDB, where games, mods and
  texture packs are downloaded) and the update check. Without it the program plays local worlds
  and joins a server whose address is typed in. The update checker switch sets only the default of
  the `update_information_url` setting, so with cURL off it is named for completeness.
- `-DENABLE_GETTEXT=OFF` — no gettext, so the engine's interface is English only and its message
  catalogues are not built, in line with the system's
  [language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy).
- `-DENABLE_SOUND=ON` — in-game sound through [openal-soft](#port-openal-soft) and Ogg Vorbis.
  Upstream's default; with it on, a missing OpenAL or Vorbis is a configure error.
- `-DENABLE_OPENSSL=ON` — take SHA-1 and SHA-2 from OpenSSL's libcrypto, which upstream describes
  as faster than its own. Upstream's default, named.
- `-DREQUIRE_LUAJIT=ON` — make [luajit](#port-luajit) required. Upstream looks for LuaJIT and, when
  it is missing, compiles the plain Lua interpreter bundled in the source instead, which is slower;
  with this switch a missing LuaJIT is a configure error.
- `-DENABLE_POSTGRESQL=OFF` — with `-DENABLE_LEVELDB=OFF` and `-DENABLE_REDIS=OFF`: store maps in
  SQLite only, the backend that is always built. These three are alternative map databases, each on
  by default when its library is found; the switches keep the result from depending on what the
  build root holds.
- `-DENABLE_SPATIAL=OFF` — no libspatialindex for area stores, which upstream uses to speed them up
  when it finds the library.
- `-DENABLE_PROMETHEUS=OFF` — no Prometheus metrics exporter, upstream's default, named.
- `-DENABLE_CURSES=OFF` — no ncurses server console. Its default follows `BUILD_SERVER`, which is
  off.
- `-DINSTALL_DEVTEST=OFF` — do not install Development Test, upstream's test game. Upstream's
  default, named.

## Notes

`minetest_game-<version>.tar.gz` unpacks beside Luanti's own tree, and `build.sh` copies it whole
into `/usr/share/luanti/games/minetest_game`, where the engine finds the games it offers in its
main menu. The game's version is the `_mtg` helper in `kpkgbuild`, and it moves separately from the
engine's. `build.sh` then deletes every `.tr` file under the game's `locale/` directories: these are
Luanti's translation files, which the engine loads at run time, and kpkg's prune removes only
compiled catalogues.
