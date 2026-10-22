## About

MediaInfoLib is the library behind [mediainfo](#port-mediainfo): it reads a media file's container
and every track in it and reports the codec, profile, bit depth, frame rate, channel layout,
duration and tags. Its CMake build lives in `Project/CMake`, which is where `build.sh` runs it.

## Flags

- `-DBUILD_ZENLIB=0` — With `-DBUILD_ZLIB=0`, link the [libzen](#port-libzen) and zlib ports instead
  of the copies the build can compile; upstream's default off Windows, named.
- `-DCURL_NO_CURL_CMAKE=ON` — Send CMake's FindCURL straight to pkg-config. Its first step asks for
  a `CURLConfig.cmake`, which curl does not install here, and with curl required that miss would
  stop the configure; see [A graphical
  application](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-graphical-application).
- `-DCMAKE_REQUIRE_FIND_PACKAGE_CURL=ON` — Require libcurl. Without it the library links no curl and
  loads `libcurl` with `dlopen` at run time to read URLs.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_PkgConfig=ON` — Require pkg-config. Without it the build installs no
  `libmediainfo.pc`, the file a pkg-config consumer reads; the CMake package file,
  `MediaInfoLibConfig.cmake`, which mediainfo's own build finds, is installed either way.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_TinyXML=ON` — Require the [tinyxml2](#port-tinyxml2) port. Without
  it the build compiles the copy in `Source/ThirdParty/tinyxml2` into the library.

## Patches

- `musl-size_t.patch` — includes `<stddef.h>` (`<cstddef>` in C++) in the Unix branch of `MediaInfoDLL.h`, which names `size_t` throughout and declares it nowhere; glibc's `<dlfcn.h>` brings it in, musl's does not, so every consumer of the header fails on its first prototype. The header is installed, so no flag on this build would reach those consumers.
