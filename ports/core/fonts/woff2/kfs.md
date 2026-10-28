## About
woff2 is Google's reference implementation of WOFF2, the Brotli-compressed font format of the web:
the `libwoff2dec` and `libwoff2enc` libraries, the `libwoff2common` that both link, and the
`woff2_compress`, `woff2_decompress` and `woff2_info` tools. [fontforge](#port-fontforge) links
`libwoff2dec` and `libwoff2enc` to open and save WOFF2 fonts. FreeType reads WOFF2 by itself,
through [brotli](#port-brotli), and WebKitGTK is built with `-DUSE_WOFF2=OFF`, so neither needs
this port.

## Flags
- `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` — Upstream's `CMakeLists.txt` declares
  `cmake_minimum_required(VERSION 2.8.6)`, and the tree's CMake 4 refuses a floor below 3.5; the
  flag raises it without a patch (see
  [An old CMake policy floor](../../../../docs/kdos/05-developer/build-troubleshooting.md#an-old-cmake-policy-floor)).
- `-DCMAKE_SKIP_RPATH=ON` — Write no `RPATH` into any library or tool. Upstream's `CMakeLists.txt`
  sets the install `RPATH` to `CMAKE_INSTALL_LIBDIR` whenever that is not one of the compiler's
  implicit directories, and with the relative `lib` the comparison never matches, so every binary
  would carry the relative entry `lib`, searched from the caller's working directory, which the
  package lint fails (see
  [What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
- `-DBUILD_SHARED_LIBS=ON` — Build the three libraries shared. On is upstream's default.

## Notes
In a shared build upstream installs only the libraries, not the three tools;
`install-executables.patch` installs them in every build.
