## About

RTTR is a run-time reflection library for C++: a program registers its classes, their properties,
methods and constructors with `RTTR_REGISTRATION`, and can then look them up by name, read and write
properties and invoke methods on objects it knows only as a `rttr::variant`. The port installs the
shared `librttr_core`, its headers under `/usr/include/rttr`, and the CMake package files under
`/usr/share/rttr/cmake`, so that a program can find it with `find_package(rttr)` as the imported
target `RTTR::Core`. No port in the tree depends on it.

## Flags

- `-DBUILD_RTTR_DYNAMIC=ON`, `-DBUILD_STATIC=OFF` — Build the shared library, `librttr_core.so`, and
  not the static `librttr_core.a` beside it.
- `-DBUILD_WITH_STATIC_RUNTIME_LIBS=OFF` — When on, upstream builds a further variant of the
  library, `rttr_core_s`, linked against the C++ runtime statically. Off, the library uses the
  shared `libstdc++` like every other C++ library on the system.
- `-DBUILD_UNIT_TESTS=OFF`, `-DBUILD_BENCHMARKS=OFF`, `-DBUILD_EXAMPLES=OFF` — Upstream builds the
  unit tests and the examples by default. Nothing here runs or installs them.
- `-DBUILD_DOCUMENTATION=OFF` — On by default, it adds the `doc` directory, which generates the API
  reference with doxygen.
- `-DBUILD_INSTALLER=ON` — Every `install()` rule for the library sits behind this switch: the
  libraries, the headers, the exported `rttr-config.cmake` and the licence and README. Without it,
  `ninja install` installs nothing.
- `-DBUILD_PACKAGE=OFF` — On by default, it includes `CMake/installer.cmake`, which sets up CPack to
  make `.tar.gz` and `.tar.bz2` archives of the build. kpkg makes the package, so CPack has nothing
  to do.

## Patches

- `permission.patch` — Drops `PERMISSIONS OWNER_READ` from the `install(FILES …)` rule for the public headers in `CMake/utility.cmake`, so the headers are installed with CMake's default mode, readable by everyone, instead of readable by their owner alone.

## Notes

The header patch does not cover the licence and README, which `CMake/config.cmake` installs into
`/usr/share/rttr` with `PERMISSIONS OWNER_READ` as well. The `find … -exec chmod 644` line at the
end of `build.sh` gives the plain files there, and only there (`-maxdepth 1`), mode 644 instead.
