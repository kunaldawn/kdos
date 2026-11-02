## About

libphonenumber is Google's library for parsing, formatting and validating international phone
numbers, with the per-country metadata compiled in. This port builds the C++ library alone, from
the `cpp/` directory of a source tree that also holds the Java and JavaScript versions. Its
consumer is [kitinerary](#port-kitinerary), which finds it through the CMake package
`libphonenumber` and links `libphonenumber::phonenumber-shared` to read phone numbers out of
booking confirmations.

## Flags

- `-D CMAKE_INSTALL_LIBDIR=lib` — The project uses `GNUInstallDirs`, whose default on x86-64 is
  `lib64`, which the package lint refuses.
- `-D BUILD_GEOCODER=OFF` — No offline geocoder (`libgeocoding`), which maps a number to a place
  name. Building it compiles a generator and a large data table of its own, and no consumer in the
  tree links it.
- `-D REGENERATE_METADATA=OFF` — Use the metadata already compiled into the source tree. Upstream's
  default regenerates it from the XML with a Java tool, which needs a JDK at build time.
- `-D BUILD_STATIC_LIB=OFF` — Build the shared library only. Upstream builds a static archive from
  the same sources beside it; nothing in the tree links it statically, and the exported CMake
  targets then name only `phonenumber-shared`, the one kitinerary looks for first.
- `-D USE_BOOST=OFF` — Upstream's default uses Boost's threads and date-time for its locks. Off,
  with `-D USE_STDMUTEX=ON`, the library uses `std::mutex` and links no Boost.
- `-D USE_STDMUTEX=ON` — The lock implementation once Boost is off: C++11 `std::mutex`. With
  neither, `lock.h` falls back to its POSIX threads implementation on Linux and the library links
  the threads library itself.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_absl=ON` — Make the system Abseil from
  [abseil-cpp](#port-abseil-cpp) a requirement. `find_package(absl)` is optional upstream, and
  when it fails the configure fetches Abseil with `FetchContent` from GitHub, which the offline
  build cannot reach.

## Notes

`protoc` from [protobuf](#port-protobuf) generates `phonemetadata.pb.cc` and `phonenumber.pb.cc`
during the build, and the installed headers include protobuf's and Abseil's, so both stay in
`depends` for every consumer. The installed `libphonenumber-config.cmake` calls
`find_dependency(absl)` and `find_dependency(Protobuf)` for the same reason. ICU's `icuuc` and
`icui18n` from [icu](#port-icu) are found with `find_library` and are required.

`LICENSE.Chromium` covers the few files under `cpp/src/phonenumbers/base/` taken from Chromium
(BSD-3-Clause); `kpkg` copies it with `LICENSE`.
