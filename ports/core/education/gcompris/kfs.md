## About

GCompris is a suite of educational activities for children aged 2 to 10: reading, counting,
telling the time, geography, science experiments, puzzles, games and drawing. It is a Qt Quick
program, `gcompris-qt`. The port ships the activities' word pictures, the English voices and the
background music with it, so the activities that use them work with no network.

## Flags

- `-D CMAKE_TOOLCHAIN_FILE="$SRC_ROOT/no-qml-rpath.cmake"` — A one-line toolchain file that sets
  `QT_NO_QML_PLUGIN_RPATH`, which stops Qt giving each QML plugin an `$ORIGIN` run path. It is a
  toolchain file and not a `-D` switch because the Box2D plugin is built as a CMake external
  project that receives from the outer build only its toolchain file, prefix paths and build
  type (see
  [What the package lint refuses](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
- `-D WITH_DOWNLOAD=OFF` — Build without the internal downloader, which is on by default. Nothing
  reaches for the network, and the settings page offers no download.
- `-D SKIP_TRANSLATIONS=ON` — Do not build the interface catalogues.
- `-D BUILD_SERVER=OFF` — Do not build the teachers' server, which upstream builds by default.
  Upstream packages it only when `PACKAGE_SERVER` is on, which is off by default, so it would be
  built and never installed.
- `-D COMPILE_DOC=OFF` — No docbook manual. It is upstream's default.
- `-D QML_BOX2D_MODULE=submodule` — Build the qml-box2d physics module from `external/qml-box2d`.
  The default, `auto`, uses a system copy when one is found; there is no qml-box2d port, so
  naming the submodule keeps configure from searching.
- `-D COMPRESSED_AUDIO=ogg` — The voices and sounds are Ogg Vorbis. It is upstream's default on
  Linux. The download manager looks for the voices under `voices-<format>`, so the value must
  match the `voices-ogg` packs the recipe fetches.

## Notes

qml-box2d, the physics module several activities import, is a git submodule that the release
archive carries empty. The recipe's second source is qml-box2d at a pinned commit, the recipe's
`_box2d`, and `build.sh` copies it into `external/qml-box2d` before configure.
`cmake/box2d.cmake` builds it as an external project and installs it under `/usr/lib/qml`, which
`gcompris-qt` adds to its import path.

The word pictures, the two English voice packs and the background music are the `.rcc` resource
files the downloader would otherwise fetch on first use. Each is a source of the recipe, pinned by
its date stamp. `build.sh` installs each one under `/usr/share/gcompris-qt/rcc/data3/<kind>/` and
appends its md5 to that directory's `Contents` file, the list the download manager reads to know
what is present.

The menu entry is replaced: `gcompris-qt` sets its desktop file name, and so its Wayland app_id,
to `org.kde.gcompris`, which the entry gives as `StartupWMClass`.
