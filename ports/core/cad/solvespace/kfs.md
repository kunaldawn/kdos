## About
[SolveSpace]{.idx} is a parametric CAD program: a part is drawn as 2D sketches held by
constraints, which its solver keeps satisfied as dimensions change, then extruded, lathed or
combined into a solid and exported as DXF, STEP, STL or a mesh. The port builds its Qt 6 window,
`solvespace-qt`, and the command-line `solvespace-cli`, which exports and regenerates files without
a display.

## Flags
- `-DUSE_QT_GUI=ON` — build the graphical program on Qt 6 (`find_package(Qt6 … OpenGLWidgets
  Widgets)`), installed as `solvespace-qt`. Upstream's default is off, which on Linux builds the
  GTK 3 interface through gtkmm and json-c instead.
- `-DENABLE_GUI=ON` — build a graphical program at all. It is upstream's default, named beside
  `USE_QT_GUI`.
- `-DENABLE_CLI=ON` — build `solvespace-cli`, upstream's default on a native build. Because the
  command-line program renders through Cairo, this is what makes [cairo](#port-cairo) a required
  dependency even with the Qt interface.
- `-DENABLE_TESTS=OFF` — leave out the test suite, which upstream builds and runs by default on a
  native build.
- `-DENABLE_OPENMP=ON` — parallelise the geometric operations with OpenMP. Off by default upstream.
- `-DENABLE_LTO=OFF` — no interprocedural optimisation; upstream's default, named.
- `-DFORCE_VENDORED_Eigen3=OFF` — prefer the system Eigen to the copy under `extlib/eigen`.
  Upstream's default, named.
- `-DEIGEN3_INCLUDE_DIRS=/usr/include/eigen3` — where the system Eigen's headers are. SolveSpace
  uses the system copy only when `find_package(Eigen3 CONFIG)` leaves this variable set, and the
  [eigen](#port-eigen) port's package configuration defines only the `Eigen3::Eigen` target, not
  the variable; without the switch the build would fall back to the bundled copy in silence.
- `-DCMAKE_DISABLE_FIND_PACKAGE_SpaceWare=ON` — skip the search for `libspnav`, the 3D-mouse
  library (upstream's `cmake/FindSpaceWare.cmake`). The search is unconditional, so without the
  switch the program would link [libspnav](#port-libspnav) whenever it happened to be in the
  build root, and it is not in `depends`.

## Patches
- `eigen5-cxx14.patch` — raises `CMAKE_CXX_STANDARD` in the top `CMakeLists.txt` from 11 to 14. The system Eigen stops with `#error Eigen requires at least c++14 support.` in `Eigen/src/Core/util/Macros.h` under any older standard, and SolveSpace pins its standard with `CMAKE_CXX_STANDARD_REQUIRED`, so no compiler default can lift it.

## Notes
The release archive carries its own copies of several libraries under `extlib/`. On Linux the build
takes zlib, libpng, FreeType and Cairo from the system, and Eigen through the two switches above;
the allocator, mimalloc, and the DXF library, libdxfrw, are always the bundled ones, compiled into
the programs.

Upstream installs a `solvespace-qt.desktop` generated from `res/freedesktop/solvespace.desktop.in`.
`build.sh` writes over it, adding `StartupWMClass=solvespace-qt` and its own comment, categories
and keywords.
