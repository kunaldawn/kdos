## About
libUvula unwraps a 3D model's surface into UV coordinates and projects onto it; it is what
[Cura](#port-cura)'s paint tool uses, through the `pyUvula` Python module this port builds. Only
the module ships: the C++ library is built static and linked into it. [uranium](#port-uranium),
Cura's framework, depends on it too.

## Flags
- `-Dstandardprojectsettings_DIR` — Where CMake finds `standardprojectsettings`, a package of
  compiler settings Ultimaker's CMake files load and that only Conan's Ultimaker index carries.
  `build.sh` writes a one-line config file in `$SRC_ROOT/shim` that includes
  `StandardProjectSettings.cmake` from the index tarball, the recipe's second source.
- `-Dclipper_DIR` — Where CMake finds `clipper`. The project links the
  [polyclipping](#port-polyclipping) library as `clipper::clipper`, a target name only Conan's
  recipe makes; the shim's `clipper-config.cmake` defines it from polyclipping's pkg-config file.
  Nothing of the shim reaches the package.
- `-DCMAKE_CXX_STANDARD=20` — The C++ standard the project is written in, and the lowest its
  Conan recipe accepts. Upstream, Conan's toolchain sets it; without the flag the compiler's
  default applies.
- `-DCMAKE_POSITION_INDEPENDENT_CODE=ON` — Compile the static `libuvula` as position-independent
  code, so it can be linked into the shared Python module.
- `-DUVULA_VERSION` — The library's version, which configure stops without; `-DPYUVULA_VERSION`
  sets the module's, which otherwise reads `1.0.0`. Both are the port's version.
- `-DWITH_PYTHON_BINDINGS=ON` — Build `pyUvula`. `-DWITH_JS_BINDINGS=OFF` builds no
  Emscripten module, and `-DWITH_CLI=OFF` no `uvula` command, which would need assimp and
  cxxopts.
- `-DEXTENSIVE_WARNINGS=OFF` — Compile without upstream's long warning set, which is on by default.
- `-Dpybind11_DIR` — pybind11's CMake directory, asked of the installed
  [python3-pybind11](#port-python3-pybind11) with `python3 -m pybind11 --cmakedir`.

## Notes
The project has no install rules, because upstream's Conan recipe copies the module itself, so
`build.sh` builds only the `pyUvula` target and copies `pyUvula*.so` into the interpreter's
`site-packages`.
