## About
[Netgen]{.idx} generates tetrahedral volume meshes, and triangle and quadrilateral surface meshes,
from CSG, STL and [OpenCASCADE](#port-opencascade) geometry. KDOS builds it for
[FreeCAD](#port-freecad), whose FEM workbench meshes parts with it (`-DBUILD_FEM_NETGEN=ON` in
FreeCAD's `build.sh`): the port installs `nglib`, the C++ library, its headers and CMake files, and
the `netgen` Python module that FreeCAD drives. Netgen's own Tcl/Tk window is not built.

## Flags
- `-DNETGEN_VERSION_GIT=v$version-0-g$_commit` — The version string Netgen's build reads from
  `git describe` when it is not given one. The tarball carries no `.git` and no `version.txt`,
  so without the switch the configure needs git, finds no repository, and falls back to
  `v6.2.0-0` with a warning, and the headers and the Python metadata say 6.2.0. The recipe's `_commit`
  is the commit the release tag points at, and the `-0-` says the tree is the tag itself, which
  makes the reported version the short one, the release number. `version-git.patch` is what makes
  the build-time step see the value.
- `-DUSE_SUPERBUILD=OFF` — Configure and build Netgen directly. The default super-build wraps
  Netgen in an external project of its own, runs `git submodule update --init --recursive` when
  the pybind11 submodule is empty, which it is in the release tarball, and adds a step that
  checks the submodules on every build. It downloads OpenCASCADE and zlib only under
  `BUILD_OCC` and `BUILD_ZLIB`, both off.
- `-DUSE_NATIVE_ARCH=OFF` — Compile without `-march=native`, which is on by default and would tune
  the binaries to the processor of the machine that built them. Netgen adds it as a `PUBLIC`
  option of `ngcore`, so it would also reach the compile line of every program that builds
  against Netgen's exported CMake targets.
- `-DUSE_GUI=OFF` — Build no `netgen` GUI and no `nggui` library; that interface is Tcl/Tk with
  Togl. FreeCAD uses only the library and the Python module.
- `-DUSE_PYTHON=ON`, `-DPREFER_SYSTEM_PYBIND11=ON`, `-Dpybind11_DIR="$_pybind11_dir"`,
  `-DPython3_EXECUTABLE=/usr/bin/python3` — Build the Python module against the
  [pybind11](#port-python3-pybind11) port, found through the CMake directory that
  `pybind11.get_cmake_dir()` reports. The bundled copy, `external_dependencies/pybind11`, is a git
  submodule and empty in the release tarball; with the system copy, Netgen also installs no
  pybind11 headers of its own.
- `-DUSE_OCC=ON` — Build the OpenCASCADE geometry kernel interface (`libsrc/occ`), which meshes
  OCCT shapes. It is upstream's default; [opencascade](#port-opencascade) is in `depends` for it.
- `-DUSE_MPI=OFF`, `-DUSE_CGNS=OFF`, `-DUSE_NUMA=OFF` — Build without MPI parallelization,
  without CGNS file reading and writing, and without NUMA-aware code.
- `-DUSE_JPEG=OFF`, `-DUSE_MPEG=OFF` — Build without the GUI's snapshot (libjpeg) and video
  recording (FFmpeg) features, which have no use with `USE_GUI=OFF`.
- `-DUSE_CCACHE=OFF`, `-DINSTALL_PROFILES=OFF`, `-DENABLE_UNIT_TESTS=OFF`,
  `-DBUILD_STUB_FILES=OFF` — Do not launch the compiler through ccache, install no
  `/etc/profile.d` script setting `PATH` and `NETGENDIR`, build no Catch unit tests, and do
  not run `pybind11-stubgen` to write `.pyi` stubs for the Python module.
- `-DNG_INSTALL_DIR_BIN=bin`, `-DNG_INSTALL_DIR_LIB=lib`, `-DNG_INSTALL_DIR_CMAKE=lib/cmake/netgen`,
  `-DNG_INSTALL_DIR_INCLUDE=include/netgen`, `-DNG_INSTALL_DIR_RES=share` — Netgen's own
  install layout, relative to the prefix. The one that differs from upstream's default is the
  header directory, `include/netgen` instead of `include`: Netgen installs its headers in
  subdirectories with names as generic as `core`, `general` and `include`, which would otherwise
  sit directly in `/usr/include`.

## Patches
- `version-git.patch` — Passes `NETGEN_VERSION_GIT` on to the `ng_generate_version_file` target, which reruns `cmake/generate_version_file.cmake` at build time with only `-DBDIR` and so would otherwise call `git describe` again and write the fallback version into `netgen_version.hpp`.
- `config-no-source-dir.patch` — Writes an empty `NETGEN_SOURCE_DIR` into the installed `NetgenConfig.cmake` instead of `@PROJECT_SOURCE_DIR@`, the build's source directory, a path under kpkg's work directory that does not exist on the installed system.
