## About
[LabPlot]{.idx} is KDE's program for plotting and analysing scientific data: spreadsheets and
matrices, 2D plots, curve fitting, Fourier transforms and filters, and live data read from files,
sockets, a serial port or MQTT. It imports a long list of file formats and embeds
[cantor](#port-cantor) worksheets, so a computer-algebra or R session can sit beside the plots.

## Flags
- `-D_GNU_SOURCE` — added to `CFLAGS`. LabPlot's numeric library calls `exp10`, a GNU extension that
  musl declares only under `_GNU_SOURCE`. Upstream adds the macro for Clang, Intel and PGI, not for
  GCC.
- `-D ENABLE_SDK=OFF` — do not build or install the SDK, the library and headers for writing
  programs against LabPlot's backend. `-D ENABLE_TESTS=OFF` is LabPlot's own name for leaving out
  its tests.
- `-D ENABLE_REPRODUCIBLE=ON` — defines `REPRODUCIBLE_BUILD`, which leaves the compile date and
  time out of the About dialog, so two builds of the recipe produce the same binary.
- `-D ENABLE_CANTOR=ON` — this switch and the other `ENABLE_` switches set `ON` (FFTW, HDF5,
  NetCDF, FITS, libcerf, liborigin, ROOT, Matio, MQTT, QtSerialPort, QtSvg, Discount, XLSX, MCAP,
  Orcus, Eigen3) name each optional feature. Each is upstream's default, and none of them makes a
  missing library an error; the `CMAKE_REQUIRE_FIND_PACKAGE_` switches below do that. The Origin,
  MCAP and XLSX readers are compiled from the copies of liborigin, the MCAP headers and QXlsx under
  `src/3rdparty`.
- `-D ENABLE_READSTAT=OFF` — no ReadStat importer (SAS, Stata and SPSS files). ReadStat is not a
  port, and without a system copy upstream downloads and builds its own during the build.
- `-D ENABLE_VECTOR_BLF=OFF` — no reader for Vector's BLF bus logs. With the switch on, upstream
  looks for the `Vector_BLF` and `dbc` libraries through `pkg-config`, and neither is a port.
- `-D LOCAL_MCAP_DOWNLOAD=OFF` — use the MCAP library included in the source rather than a
  download. It is upstream's default.
- `-D LOCAL_QT_ADVANCED_DOCK_DOWNLOAD=OFF` — with `-D LOCAL_QT_ADVANCED_DOCK_COPY=ON`: build the
  Qt Advanced Docking System, which the main window requires, from the copy under
  `src/3rdparty/Qt-Advanced-Docking-System` rather than fetching it from git. The library is not a
  port of its own.
- `-D QT_FIND_PRIVATE_MODULES=ON` — finding `Qt6Gui` loads `Qt6GuiPrivate` as well. The docking
  framework and the XLSX reader are built against Qt's private API, and without the target the
  configure stops.
- `-D EIGEN3_FOUND=ON` — with `EIGEN3_INCLUDE_DIR` and `EIGEN3_VERSION_STRING`, read from
  `eigen3.pc`. [eigen](#port-eigen)'s CMake package defines the `Eigen3::Eigen` target but none of
  the three variables LabPlot reads, so they are given by hand; the requirement on `Eigen3` below
  still stops the configure when the library is missing.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON` — never look for KUserFeedback, so no usage
  statistics can be collected.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_FFTW3=ON` — this switch and the other `CMAKE_REQUIRE_FIND_PACKAGE_`
  switches (HDF5, CFitsio, Poppler, Discount, Eigen3, Qt6SerialPort, Qt6Svg, KF6DocTools,
  KF6SyntaxHighlighting, KF6Purpose, LIBCERF, Matio, Orcus, Qt6Mqtt) stop the configure when that
  library is missing, where upstream would build without the feature it backs. Poppler's Qt 6
  library renders LaTeX labels, and Discount renders Markdown notes.

## Patches
- `musl-execinfo.patch` — in `src/backend/lib/trace.h`, include `<execinfo.h>` and define `print_callstack()` only under glibc. musl has no `<execinfo.h>`, and the function is a debugging aid that nothing in the build calls.
- `elapsed-timer-include.patch` — add `#include <QElapsedTimer>` to `XYFourierFilterCurve.cpp`, which uses the class; none of the Qt headers it includes brings it in.

## Notes
Cantor and NetCDF are checked after the configure, by looking for `-DHAVE_CANTOR_LIBS` and
`-DHAVE_NETCDF` in `build/build.ninja`, instead of being required with
`CMAKE_REQUIRE_FIND_PACKAGE_`. Their find modules pass `<Name>_FIND_REQUIRED` on to a
config-package search as a bare argument; a requirement sets it to `1`, which that search reads as
a version (and netCDF 4 is then incompatible) or rejects (Cantor).

The desktop entry is KDOS's own, written over upstream's. It sets
`StartupWMClass=org.kde.labplot`, the Wayland app id, and `Icon=labplot`, the name of the PNG
icons the port installs, and claims only `application/x-labplot`, the one of upstream's types that
the port's `labplot.xml` defines.
