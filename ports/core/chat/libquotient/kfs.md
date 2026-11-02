## About

libQuotient is a Qt library for writing Matrix clients: it logs in, keeps the sync loop running,
models rooms, members and timeline events, uploads and downloads media, and runs end-to-end
encryption over [olm](#port-olm), with keys stored in a local SQLite database and the access token
kept through [qtkeychain](#port-qtkeychain). NeoChat requires it, and Itinerary uses it. The
library is `libQuotientQt6`, found with `find_package(QuotientQt6)`.

The port takes 0.9.6.1, not 0.9.6: the 0.9.6 tarball breaks the ABI of the 0.9 series, which
0.9.6.1 keeps, and upstream asks packagers not to ship 0.9.6.

## Flags

- `-D CMAKE_INSTALL_LIBDIR=lib` — libQuotient is a plain CMake project that includes
  `GNUInstallDirs` itself.
- `-D BUILD_SHARED_LIBS=ON` — Build a shared library. The project declares its library with no
  type, so CMake's default would make it a static archive that every client copies into itself.

## Notes

libolm, the encryption library this release series is built on, is deprecated upstream; the 0.9
series is the last to use it, and the development series moves to the Rust SDK's crypto. A bump
past 0.9 changes the dependency, not only the version. The library links `libcrypto` from
[openssl](#port-openssl), and keeps its encryption state through Qt SQL's `QSQLITE` driver from
[qt6-qtbase](#port-qt6-qtbase).

The install writes an Android NDK makefile, `/usr/share/ndk-modules/Android.mk`, with no switch to
leave it out; nothing on KDOS reads it, and `build.sh` removes the directory. The pkg-config file
`QuotientQt6.pc` names `-lQuotient`, which is not the library's name; consumers find the library
through its CMake package, which is right.
