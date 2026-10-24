## About

[ioquake3]{.idx} is the maintained id Tech 3 engine, the Quake III Arena engine carried forward
with SDL, OpenAL, VoIP, HTTP downloads and current renderers. It installs the client `ioquake3`,
the dedicated server `ioq3ded`, both renderers and the game modules. The port builds from a commit
of upstream's repository rather than a release, named by the `_commit` helper, and its version is
the last release number followed by a date. The game data is not free and is not shipped: a
player puts `pak0.pk3` and the point-release paks under `baseq3` in the engine's data directory,
`${XDG_DATA_HOME:-~/.local/share}/Quake3`. The desktop entry's comment names `~/.q3a/baseq3`, the
engine's older location: when `~/.q3a` exists and `~/.config/Quake3` does not, the engine offers once to copy it to the XDG
directories and, if the player declines, keeps using it.

## Flags

- `-DCMAKE_INSTALL_PREFIX=/usr/lib/ioquake3` — install the engine's whole tree, programs,
  renderers and game modules together, under `/usr/lib/ioquake3` rather than `/usr`. The engine
  takes its own directory from `argv[0]` and loads the renderers and the game modules from there.
- `-DPRODUCT_VERSION=$version` — the version the engine reports. Left unset, upstream takes the
  project's version and, in a git checkout that is not on a release tag, appends `_g` and the short
  commit hash; an unpacked archive has no repository, so the snapshot would report only the release
  number.
- `-DBUILD_CLIENT=ON` — with `-DBUILD_SERVER=ON`, `-DBUILD_RENDERER_GL1=ON`,
  `-DBUILD_RENDERER_GL2=ON`, `-DBUILD_GAME_LIBRARIES=ON` and `-DBUILD_GAME_QVMS=ON`: the client, the
  dedicated server, the OpenGL 1 and OpenGL 2 renderers, and the game modules both as native
  libraries and as QVM bytecode. All are upstream's defaults, named.
- `-DBUILD_STANDALONE=OFF` — build the engine for Quake III Arena itself rather than for a
  standalone game on the engine. Upstream's default, named.
- `-DUSE_RENDERER_DLOPEN=ON` — load the renderer as a separate library at run time, so the player
  can switch between them. Upstream's default, named.
- `-DUSE_OPENAL=ON` — with `-DUSE_OPENAL_DLOPEN=ON`: sound through OpenAL, loaded at run time from
  [openal-soft](#port-openal-soft). Upstream's defaults, named.
- `-DUSE_HTTP=ON` — download the files a server's game needs over HTTP, through
  [curl](#port-curl). Upstream's default, named.
- `-DUSE_CODEC_VORBIS=ON` — with `-DUSE_CODEC_OPUS=ON`, `-DUSE_VOIP=ON` and `-DUSE_MUMBLE=ON`:
  Ogg Vorbis and Opus audio, in-game voice chat, and Mumble's positional-audio link. All are
  upstream's defaults, named.
- `-DUSE_FREETYPE=ON` — render text with [freetype2](#port-freetype2). Upstream's default is off.
- `-DUSE_INTERNAL_LIBS=OFF` — with `-DUSE_INTERNAL_SDL=OFF`, `-DUSE_INTERNAL_ZLIB=OFF`,
  `-DUSE_INTERNAL_JPEG=OFF`, `-DUSE_INTERNAL_OGG=OFF`, `-DUSE_INTERNAL_VORBIS=OFF` and
  `-DUSE_INTERNAL_OPUS=OFF`: link the tree's SDL2, zlib, libjpeg, Ogg, Vorbis and Opus rather than
  the copies in `thirdparty/`. Each per-library switch defaults to `USE_INTERNAL_LIBS`, which is
  on upstream.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_CURL=ON` — with `-DCMAKE_REQUIRE_FIND_PACKAGE_OpenAL=ON`: make the
  cURL and OpenAL lookups required. Upstream looks for each `QUIET` and, when it is missing, falls
  back to the headers bundled in `thirdparty/`, so the build would succeed without the system's.
- `-DCURL_NO_CURL_CMAKE=ON` — keep CMake's FindCURL from first looking for a `CURLConfig.cmake`,
  which the tree's curl does not install; with the lookup required, that search would itself fail
  the configure (see
  [A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).

## Notes

Because the engine finds its renderers and modules beside the path it was started by,
`/usr/bin/ioquake3` and `/usr/bin/ioq3ded` are small scripts that start the programs by their full
paths under `/usr/lib/ioquake3`; a symbolic link would leave the engine looking in `/usr/bin`.

Upstream's icon is an SVG. The desktop's icon lookup reads PNGs only (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)),
so `build.sh` renders a 256-pixel PNG from `misc/quake3.svg` with `rsvg-convert` from
[librsvg](#port-librsvg) and writes the desktop entry; the window's Wayland app id is the
executable's name, `ioquake3`.
