## About
[minizip-ng]{.idx} is the zlib-ng project's fork of minizip, a ZIP library with its own API,
`<minizip-ng/mz_zip.h>` and the rest. It reads and writes the deflate, bzip2, xz, zstd and PPMd
methods, and both PKWARE and WinZip AES encryption. [opencolorio](#port-opencolorio) links it.
zlib's own minizip is the separate [minizip](#port-minizip) port.

## Flags
- `-DMZ_COMPAT=OFF` — install the library and headers under minizip-ng's own names,
  `libminizip-ng` and `<minizip-ng/…>`. The compatibility layer, upstream's default, would install
  a `libminizip` and `<unzip.h>` that collide with the `minizip` port's.
- `-DMZ_ZLIB_FLAVOR=zlib` — link [zlib](#port-zlib). Left at `auto`, minizip-ng looks for zlib-ng
  first, and the [zlib-ng](#port-zlib-ng) port, which installs a CMake package, is in the same
  phase without being in `depends`, so the library linked would depend on build order.
- `-DMZ_FETCH_LIBS=OFF` — with `-DMZ_FORCE_FETCH_LIBS=OFF`: never download a compression library.
  A library that is not found disables its method instead. Both are upstream's defaults on Linux.
- `-DMZ_PPMD=ON` — the PPMd method. There is no PPMd library to find: minizip-ng compiles three
  source files of 7-Zip's for it, and clones 7-Zip to get them whatever `MZ_FETCH_LIBS` says.
- `-DFETCHCONTENT_SOURCE_DIR_PPMD` — points that clone at the 7-Zip release unpacked from the
  recipe's second source, the same tarball, under the same name and hash, as the
  [7zip](#port-7zip) port's, so the source archive holds the file once.
- `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` — make any other download CMake's `FetchContent` would
  attempt an error instead of a network call.
- `-DMZ_ZSTD=ON` — the zstd method, with [zstd](#port-zstd) found as `zstd-pkgconfig.patch` lets
  it be.
- `-DMZ_LIBBSD=OFF` — do not look for libbsd's `arc4random_buf`. Upstream's default on Unix is
  on, but its random-number probe runs only when OpenSSL is off; with `MZ_OPENSSL`, the random
  bytes for encryption come from OpenSSL's `RAND_bytes`, and the switch changes nothing.
- `-DMZ_OPENSSL=ON` — with `-DMZ_PKCRYPT=ON` and `-DMZ_WZAES=ON`: PKWARE's traditional
  encryption and WinZip AES, with the cryptography from [openssl](#port-openssl).
- `-DMZ_ICONV=ON` — convert file-name encodings with `iconv`, which musl provides itself, so no
  library is added.

## Patches
- `zstd-pkgconfig.patch` — when `find_package(zstd CONFIG)` finds nothing, looks for `libzstd` through pkg-config and links the imported `PkgConfig::ZSTD_PC` target. minizip-ng otherwise finds zstd only as a CMake package, and the [zstd](#port-zstd) port, built with zstd's plain makefiles, installs `libzstd.pc` and no CMake package, so the method would be silently disabled.

## Notes
The `readelf` line at the end of `build.sh` fails the build unless `libminizip-ng.so` needs
`libzstd`, so a zstd that neither CMake nor pkg-config finds cannot ship a library without the
method.

minizip-ng's `CMakeLists.txt` names the 7-Zip version it clones for PPMd; the recipe's `_7zip` is
the [7zip](#port-7zip) port's version, which can be a later release than that pin.

The package also installs `libppmd.a`, the static archive the build makes of those 7-Zip sources:
minizip-ng's exported CMake targets list it as `MINIZIP::ppmd`, so a consumer that finds the
package through CMake needs it to be present.
