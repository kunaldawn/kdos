## About
RapidJSON is Tencent's header-only JSON parser and generator for C++, with both a DOM and a SAX
(streaming) API and in-place parsing. It installs only headers, a CMake package configuration and
a pkg-config file.
[kodi](#port-kodi), [ppsspp](#port-ppsspp), [octave](#port-octave), [opencascade](#port-opencascade),
[openbabel](#port-openbabel), [opencpn](#port-opencpn) and [curaengine](#port-curaengine) build
against it.

## Flags
- `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` — The `CMakeLists.txt` declares
  `CMAKE_MINIMUM_REQUIRED(VERSION 2.8)`, and the tree's CMake 4 refuses a floor below 3.5 (see
  [An old CMake policy floor](../../../../docs/kdos/05-developer/build-troubleshooting.md#an-old-cmake-policy-floor)).
- `-DRAPIDJSON_BUILD_DOC=OFF` — Do not build the Doxygen documentation. Upstream's default is on.
- `-DRAPIDJSON_BUILD_EXAMPLES=OFF` — Do not build the example programs. Upstream's default is on.
- `-DRAPIDJSON_BUILD_TESTS=OFF` — Do not build the unit and performance tests, which need
  googletest. Upstream's default is on.

## Patches
- `prettywriter-writeflags.patch` — Two fixes to `PrettyWriter` in `prettywriter.h`: its `Base` typedef passes the class's `writeFlags` template argument on to `Writer`, so flags such as `kWriteNanAndInfFlag` given to a `PrettyWriter` take effect, and its allocator constructor initialises `formatOptions_`, which that constructor otherwise leaves unset.

## Notes
`gcc14.patch` and `CVE-2024-38517.patch` are upstream commits that the release does not contain.
The first removes an assignment operator of `GenericStringRef` that assigns to its `const`
members and so does not compile with GCC 14; the second keeps the exponent arithmetic of the
number parser from underflowing `int` on a very large negative exponent.

The install copies `include/rapidjson` as a directory, whatever is in it. `patch` therefore runs
with `--no-backup-if-mismatch`, so a hunk applied with fuzz leaves no `.orig` file to be installed
as a header, and the `test -z "$(find … -name '*.orig' -o -name '*.rej')"` line stops the build if
one is there anyway. Upstream's `readme.md` and the example sources it installs under
`/usr/share/doc` are deleted.
