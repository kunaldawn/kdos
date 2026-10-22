## About

Eigen is a C++ template library for linear algebra: dense and sparse matrices, vectors,
decompositions and solvers, all in headers. Nothing is compiled for it to be used; a consumer
includes it from `/usr/include/eigen3` and the code is generated in that consumer. In KDOS it is
built into [blender](#port-blender), [freecad](#port-freecad), [krita](#port-krita),
[opencv](#port-opencv), [vtk](#port-vtk), [cgal](#port-cgal), [ceres-solver](#port-ceres-solver)
and [nextpnr](#port-nextpnr), among others. The build only configures and installs the headers,
the CMake package files and the pkg-config file.

## Flags

- `-DEIGEN_BUILD_BLAS=OFF`, `-DEIGEN_BUILD_LAPACK=OFF` — Do not build Eigen's own BLAS and LAPACK
  libraries (`libeigen_blas`, `libeigen_lapack`), which upstream builds by default when Eigen is
  the top-level project. The tree's BLAS and LAPACK is [openblas](#port-openblas).
- `-DEIGEN_BUILD_DOC=OFF` — Set up no documentation targets, which run Doxygen over the headers.
- `-DEIGEN_BUILD_DEMOS=OFF` — Set up no demo programs.
- `-DEIGEN_BUILD_PKGCONFIG=ON` — Install `eigen3.pc`, so a consumer that asks pkg-config for
  `eigen3` finds the headers. Upstream's default for a top-level build.
