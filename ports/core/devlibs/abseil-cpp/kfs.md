## About
Abseil is Google's collection of C++ libraries that sit beside the standard library: strings and
formatting (`absl::StrCat`, `absl::StrFormat`), hash containers (`flat_hash_map`), synchronisation,
time, status codes and logging. [protobuf](#port-protobuf) and [re2](#port-re2) are built on it,
and through them much more, and [webrtc-audio-processing](#port-webrtc-audio-processing),
[pipewire](#port-pipewire), [blender](#port-blender), [kicad](#port-kicad),
[ceres-solver](#port-ceres-solver) and others link it directly. The port installs each Abseil
component as its own shared library, `libabsl_*.so`, each with a pkg-config file, and one CMake
package for them all.

## Flags
- `-DCMAKE_CXX_STANDARD=17`, `-DCMAKE_CXX_STANDARD_REQUIRED=ON` — Build as C++17, Abseil's
  minimum. The install rewrites `absl/base/options.h` for the standard the build used: under C++17 it
  pins Abseil's own source-location and ordering types rather than C++20's, so a consumer compiled
  as C++17 or as any later standard sees the same types the library was built with.
- `-DABSL_PROPAGATE_CXX_STD=ON` — Have every Abseil target require C++17 of what links it, through
  CMake's `cxx_std_17` compile feature, so a consumer cannot build against the headers with an
  older standard; upstream's default, written out.
- `-DCMAKE_SKIP_INSTALL_RPATH=ON` — Install the libraries without a run path. Abseil's
  `CMakeLists.txt` sets `CMAKE_INSTALL_RPATH` to `$ORIGIN`, which every installed library would
  otherwise carry.
- `-DBUILD_SHARED_LIBS=ON` — Shared libraries, one per component.
- `-DABSL_BUILD_TESTING=OFF`, `-DABSL_BUILD_TEST_HELPERS=OFF` — Neither Abseil's tests nor the
  test-helper libraries, which need GoogleTest.
- `-DABSL_ENABLE_INSTALL=ON` — Install the libraries, headers and package files; upstream's
  default when Abseil is the top-level project, written out.

## Notes
Each library's soname is taken from the Abseil release, `libabsl_base.so.2608.0.0` for
20260817.0, so a new release changes every soname and every port linked against Abseil has to be
rebuilt with it.
