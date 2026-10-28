## About

[PDAL]{.idx} (the Point Data Abstraction Library) translates and processes point clouds, such as
lidar scans and photogrammetry output: readers and writers for LAS and LAZ and other formats,
filters that are chained into pipelines, and the `pdal` command that runs them. It is built on
[gdal](#port-gdal) and [proj](#port-proj) for coordinate systems, and [qgis](#port-qgis) links it
for its point-cloud layers.

## Flags

- `-D WITH_GCS=OFF` — No reader or writer for Google Cloud Storage, a network store. Upstream's
  default is on.
- `-D WITH_ZLIB=ON`, `-D WITH_ZSTD=ON`, `-D WITH_LZMA=ON` — Compression support, through
  [zlib](#port-zlib), [zstd](#port-zstd) and [xz](#port-xz). zstd is found through pkg-config
  because of `10-zstd.patch`.
- `-D WITH_COMPLETION=OFF` — Leave the bash completion to `build.sh`; off is upstream's default.
  With it on, upstream installs the script only when `/usr/share/bash-completion/completions` (or
  else `/etc/bash_completion.d`) already exists on the build machine, so the result would depend
  on what was built first. `build.sh` installs `scripts/bash-completion/pdal` into
  `/usr/share/bash-completion/completions` itself.
- `-D WITH_BACKTRACE=OFF` — Build without the stack trace PDAL prints on a crash, which links
  libunwind or libexecinfo when found. It is on by default upstream.
- `-D WITH_ABSEIL=OFF` — No Abseil, which upstream uses only as test debugging support.
- `-D BUILD_TOOLS_LASDUMP=ON` — Build `lasdump`, which prints the header and points of a LAS file.
- `-D BUILD_TOOLS_NITFWRAP=OFF` — No `nitfwrap`, which needs the NITF library.
- `-D BUILD_PLUGIN_DRACO=ON` — Read and write Google's Draco compressed point clouds, through
  [draco](#port-draco).
- `-D BUILD_PLUGIN_HDF=ON`, `-D BUILD_PLUGIN_ICEBRIDGE=ON` — Read HDF5 point data and NASA's
  ICESat/IceBridge files, through [hdf5](#port-hdf5).
- `-D BUILD_PLUGIN_E57=ON` — Read and write ASTM E57 scanner files; the libE57Format copy the
  plugin builds parses their XML section with [xerces-c](#port-xerces-c).
- `-D BUILD_PLUGIN_PGPOINTCLOUD=ON` — Read and write point clouds stored in PostgreSQL with the
  pgpointcloud extension, through [postgresql](#port-postgresql)'s client library.
- `-D BUILD_PLUGIN_TRAJECTORY=ON` — Compute a sensor trajectory from LAS input, with
  [ceres-solver](#port-ceres-solver).
- `-D BUILD_PLUGIN_ARROW=OFF` to `-D BUILD_PLUGIN_TILEDB=OFF` — No Arrow, CPD, FBX, MATLAB, MBIO,
  NITF, OpenSceneGraph, rdblib, RiVLib, SPZ, TEASER++ or TileDB plugins. Each needs a library the
  tree does not carry. Every plugin is off by default upstream; the ones built are named on.
- `-D WITH_TESTS=OFF`, `-D BUILD_PGPOINTCLOUD_TESTS=OFF`, `-D ENABLE_CTEST=OFF`,
  `-D BUILD_DOCS=OFF` — Build no tests and no documentation targets.

## Patches

- `10-zstd.patch` — In `cmake/zstd.cmake`, replaces `find_package(zstd CONFIG QUIET)` with a required `pkg_check_modules(ZSTD libzstd)`, and in `CMakeLists.txt` links `${ZSTD_LIBRARIES}` instead of the `zstd::libzstd` target. The [zstd](#port-zstd) port is built with its makefiles and installs a pkg-config file but no CMake package, so upstream's quiet lookup fails and the library is built with no zstd support, unable to read zstd-compressed point clouds.
