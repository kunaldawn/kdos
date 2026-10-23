## About

MED-file is the library for the MED format, the mesh and field file format of the Salome
platform, stored inside HDF5 files. [freecad](#port-freecad) reads and writes FEM meshes through
it, and [gmsh](#port-gmsh) imports and exports `.med` meshes. The package holds the libraries `libmedC`, `libmed`,
`libmedfwrap` and `libmedimport`, and the `mdump`, `xmdump` and `medconforme` tools. The source is the GitHub repository `chennes/med`, which carries
MED-file's sources with both its autotools and its CMake build; the recipe uses CMake.

## Flags

- `-DCMAKE_SKIP_RPATH=ON` — Give no binary an `RPATH`, in the build tree or installed. Every
  library the tools need is in `/usr/lib`, where the loader looks anyway.
- `-DMEDFILE_BUILD_SHARED_LIBS=ON`, `-DMEDFILE_BUILD_STATIC_LIBS=OFF` — Build the shared library
  only; both are upstream's defaults.
- `-DMEDFILE_BUILD_TESTS=OFF` — Build none of the test programs, which upstream builds by default.
- `-DMEDFILE_BUILD_PYTHON=OFF` — No Python bindings; they need SWIG. Upstream's default.
- `-DMEDFILE_BUILD_DOC=OFF`, `-DMEDFILE_INSTALL_DOC=OFF` — Neither generate the API documentation
  with Doxygen nor install the pre-built HTML documentation the source carries, which upstream
  installs by default.
- `-DDESTDIR="$PKG"` — The staging directory, as a CMake variable. `tools/mdump/CMakeLists.txt`
  makes the `mdump` and `xmdump` links with `install(CODE …)` in `${DESTDIR}/usr/bin`, a variable
  expanded when CMake configures, not the `DESTDIR` environment variable the install reads, so
  without it both links are written into the build root's `/usr/bin` and the package has none.
  `build.sh` checks for both links after the install.

## Patches

- `cmake-config-dir.patch` — Installs the CMake package files (`MEDFileConfig.cmake` and the rest) under `lib/cmake/medfile-<version>/` instead of `share/cmake/medfile-<version>/`, beside the other ports' CMake package files.
- `hdf5-1.14.patch` — Fedora's patch for building MED-file against an HDF5 newer than 1.12. In `medMacros.cmake` it replaces the configure test that demands a 1.12 release from 1.12.1 on with one that demands any release from 1.12.1 on, and in the C sources it raises each `#if H5_VERS_MINOR > 12` guard, which stops the compile with `#error`, to `> 14`.

## Notes

The `#error` guards compare only `H5_VERS_MINOR` and ignore the major version. Against an
[hdf5](#port-hdf5) whose major version is above 1, as the tree's is, they pass on its small minor
number whatever bound the patch gives them; the CMake test passes because such a release is not
less than 1.12.1. What keeps the file format fixed is in the library itself: it creates files with
`H5Pset_libver_bounds(…, H5F_LIBVER_V112, H5F_LIBVER_V112)`, and on opening one it sets the bounds
to `H5F_LIBVER_V18` for a file from before MED 5.0 and to `H5F_LIBVER_V112` otherwise, so a `.med`
file it writes keeps the HDF5 file format of its MED version whatever HDF5 it is linked against.
