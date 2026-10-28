## About

[DevilutionX]{.idx} is an engine for Blizzard's Diablo and its expansion Hellfire, built on
Devilution, a reconstruction of the original game's source code, so that it runs on current
systems. It ships no game: the player
supplies their own `DIABDAT.MPQ` (and Hellfire's files for the expansion), which cannot be
redistributed. The engine's own assets, `devilutionx.mpq`, come prebuilt in upstream's source
release. This port installs the one program, `devilutionx`, with a menu entry for each game.

## Flags

- `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` — never let CMake's FetchContent download anything. The
  source is upstream's fully vendored distribution, which carries the sources of the FetchContent
  dependencies this build uses under `dist/`; with this switch a dependency missing from it is a
  configure error rather than a download, which a build with no network could not make. Upstream's
  `dist/CMakeLists.txt` leaves the switch off, because its Windows and Android builds still fetch.
- `-DDISABLE_LTO=ON` — no link-time optimisation, which upstream turns on for a release build.
- `-DDISABLE_ZERO_TIER=ON` — leave out ZeroTier multiplayer. It joins games through a public
  ZeroTier network, reached through ZeroTier's servers, and builds the libzt copy in `dist/`.
- `-DDISABLE_TCP=OFF` — with `-DNONET=OFF`: keep network support, and with it multiplayer over
  TCP, joining a host by its address. Both are upstream's defaults, named.
- `-DDISCORD_INTEGRATION=OFF` — no Discord rich presence. Upstream's default, named.
- `-DNOSOUND=OFF` — build with sound. Upstream's default, named.
- `-DBUILD_ASSETS_MPQ=OFF` — do not package the engine's assets into `devilutionx.mpq` at build
  time, which needs the `smpq` tool; the tree has no port of it. The `devilutionx.mpq` in `dist/`
  is installed instead.
- `-DDEVILUTIONX_SYSTEM_SDL2=ON` — with `-DDEVILUTIONX_SYSTEM_SDL_IMAGE=ON`,
  `-DDEVILUTIONX_SYSTEM_ZLIB=ON`, `-DDEVILUTIONX_SYSTEM_BZIP2=ON` and
  `-DDEVILUTIONX_SYSTEM_LIBSODIUM=ON`: link the tree's SDL2, SDL_image, zlib, bzip2 and
  [libsodium](#port-libsodium) rather than bundled copies. Upstream's `dist/CMakeLists.txt` sets
  the SDL2, SDL_image, bzip2 and libsodium switches to `OFF` when they are unset, so without these
  flags the source distribution compiles its own copies from `dist/`; the zlib switch is on by
  default and named. On means `find_package(... REQUIRED)`, so a missing library fails the
  configure instead of falling back to the copy.
- `-DDEVILUTIONX_SYSTEM_LIBFMT=OFF` — compile the bundled fmt 10.0 from `dist/`, the version this
  release is written against, rather than the tree's [fmt](#port-fmt). The source distribution
  sets it `OFF` itself when it is unset; named.
- `-DDEVILUTIONX_SYSTEM_SDL_AUDIOLIB=OFF` — with `-DDEVILUTIONX_SYSTEM_SIMPLEINI=OFF`: compile the
  bundled SDL_audiolib and simpleini, neither of which is a port. Named, so a copy that appears in
  the build root later is not picked up instead.

## Notes

The build runs from a `build` directory inside the source; upstream warns about a build in the
source directory itself. Libraries that have no system form at all, such as asio, libmpq and
libsmackerdec, are always compiled, from their sources under `dist/` through the CMake wrappers in
`3rdParty/`.

`build.sh` removes upstream's two desktop entries and writes its own, `devilutionx.desktop`
running `devilutionx --diablo` and `devilutionx-hellfire.desktop` running
`devilutionx --hellfire`. Both add `StartupWMClass=devilutionx`: the two games run the one
program, whose Wayland app id is SDL's default, the executable's name.
