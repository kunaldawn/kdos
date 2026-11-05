## About

LimeSuite is the driver library for LimeSDR boards. This port installs `libLimeSuite`, its headers,
`LimeUtil`, the `LimeQuickTest` board self-test and the SoapySDR module `libLMS7Support.so`. The
wxWidgets control panel built from the same tarball is the port
[limesuite-gui](#port-limesuite-gui).

## Flags

- `-DCMAKE_DISABLE_FIND_PACKAGE_FLTK=ON` — Build `LimeQuickTest` as a command-line program.
  `QuickTest/CMakeLists.txt` turns on its FLTK window whenever `find_package(FLTK)` succeeds, and
  no option controls that, so the result would otherwise depend on whether [fltk](#port-fltk)
  happens to be in the tree. With it present and this flag gone the build stops on `cairo.h`:
  `FL/Fl.H` includes it, and the include directory CMake's `FindFLTK` reports does not reach it.
- `-DENABLE_SIMD_FLAGS=none` — The default tunes the code to the build machine.
- `-DCMAKE_INSTALL_LIBDIR=lib` — Places the SoapySDR module, whose directory comes from SoapySDR's
  `GNUInstallDirs`, not from `LIB_SUFFIX`.
- `-DENABLE_GUI=OFF` — The control panel belongs to [limesuite-gui](#port-limesuite-gui).
