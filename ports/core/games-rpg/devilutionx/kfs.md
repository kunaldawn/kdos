## About

[DevilutionX]{.idx} is an engine for Blizzard's Diablo and its expansion Hellfire, rebuilt from
the original game's code so that it runs on current systems. It ships no game: the player
supplies their own `DIABDAT.MPQ` (and Hellfire's files for the expansion), which cannot be
redistributed. The engine's own assets, `devilutionx.mpq`, come prebuilt in upstream's source
release. This port installs the one program, `devilutionx`, with a menu entry for each game.

## Flags

- `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` — never let CMake's FetchContent download anything. The
  source is upstream's fully vendored distribution, which carries every FetchContent dependency
  under `dist/`; with this switch a dependency missing from it is a configure error rather than a
  download, which a build with no network could not make.
- `-DDISABLE_LTO=ON` — no link-time optimisation, which upstream turns on for a release build.
- `-DDISABLE_ZERO_TIER=ON` — leave out ZeroTier multiplayer. It joins games through a public
  ZeroTier network, reached through ZeroTier's servers, and builds the libzt copy in `dist/`.
- `-DDISABLE_TCP=OFF` — with `-DNONET=OFF`: keep network support, and with it multiplayer over TCP
  on a local network. Both are upstream's defaults, named.
- `-DDISCORD_INTEGRATION=OFF` — no Discord rich presence. Upstream's default, named.
- `-DNOSOUND=OFF` — build with sound. Upstream's default, named.
- `-DBUILD_ASSETS_MPQ=OFF` — do not package the engine's assets into `devilutionx.mpq` at build
  time, which needs the `smpq` tool; the tree has no port of it. The `devilutionx.mpq` in `dist/`
  is installed instead.
- `-DDEVILUTIONX_SYSTEM_SDL2=ON` — with `-DDEVILUTIONX_SYSTEM_SDL_IMAGE=ON`,
  `-DDEVILUTIONX_SYSTEM_ZLIB=ON`, `-DDEVILUTIONX_SYSTEM_BZIP2=ON` and
  `-DDEVILUTIONX_SYSTEM_LIBSODIUM=ON`: link the tree's SDL2, SDL_image, zlib, bzip2 and
  [libsodium](#port-libsodium) rather than the copies under `3rdParty/` and `dist/`. Each is on by
  default, and on means `find_package(... REQUIRED)`, so a missing library fails the configure
  instead of falling back to the copy.
- `-DDEVILUTIONX_SYSTEM_LIBFMT=OFF` — compile the bundled fmt, the version this release is written
  against, rather than the tree's [fmt](#port-fmt).
- `-DDEVILUTIONX_SYSTEM_SDL_AUDIOLIB=OFF` — with `-DDEVILUTIONX_SYSTEM_SIMPLEINI=OFF`: compile the
  bundled SDL_audiolib and simpleini, neither of which is a port. Named, so a copy that appears in
  the build root later is not picked up instead.

## Notes

The build runs from a `build` directory inside the source; upstream warns about a build in the
source directory itself. Copies that have no system form at all, such as asio, libmpq and
libsmackerdec, are always compiled from `3rdParty/`.

`build.sh` removes upstream's two desktop entries and writes its own, `devilutionx.desktop`
running `devilutionx --diablo` and `devilutionx-hellfire.desktop` running
`devilutionx --hellfire`. Both add `StartupWMClass=devilutionx`: the two games run the one
program, whose Wayland app id is SDL's default, the executable's name.
