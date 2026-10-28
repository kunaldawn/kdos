## About

VTK, the Visualization Toolkit, is a C++ library of data structures, filters and OpenGL rendering
for meshes, volumes and scientific data, with Python bindings. In KDOS it is there for
[FreeCAD](#port-freecad): FreeCAD's FEM workbench post-processes its results through VTK, and its
post-processing filters are VTK Python objects, so the Python wrapping is built. External libraries
are this tree's ports wherever VTK can use one, and its own bundled copy otherwise.

## Flags

- `-DLOGURU_STACKTRACES=0` — Build VTK's bundled loguru logger without stack traces. On Linux it
  prints them through `<execinfo.h>`, which musl does not have.
- `-DCMAKE_SKIP_INSTALL_RPATH=ON` — Install the libraries and modules with no RPATH. They are all
  in `/usr/lib`, where the loader looks in any case, and the package lint warns on an RPATH that
  names it (see
  [What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
- `-DVTK_BUILD_TESTING=OFF`, `-DVTK_BUILD_EXAMPLES=OFF`, `-DVTK_BUILD_DOCUMENTATION=OFF`,
  `-DVTK_USE_LARGE_DATA=OFF` — No tests, examples or Doxygen documentation, and none of the large
  test data, which VTK downloads.
- `-DVTK_USE_MPI=OFF`, `-DVTK_GROUP_ENABLE_MPI=NO` — No MPI, and none of the parallel modules
  that need it.
- `-DVTK_USE_CUDA=OFF` — No CUDA.
- `-DVTK_USE_X=ON`, `-DVTK_OPENGL_HAS_EGL=ON` — Build both the X11 (GLX) and the EGL render
  windows. VTK chooses at run time: the X11 window when a GLX context can be made, which on KDOS is
  through Xwayland, and the EGL window otherwise, so a program with no display still renders off
  screen. `VTK_DEFAULT_OPENGL_WINDOW` in the environment forces one.
- `-DVTK_OPENGL_USE_GLES=OFF` — Desktop OpenGL, not OpenGL ES.
- `-DVTK_WRAP_PYTHON=ON`, `-DPython3_EXECUTABLE=/usr/bin/python3` — Build the Python bindings,
  against the system Python. FreeCAD's FEM post-processing is built without its VTK filters when
  they are missing.
- `-DVTK_WRAP_JAVA=OFF` — No Java bindings.
- `-DVTK_GROUP_ENABLE_Qt=NO` — None of the Qt modules, VTK's Qt widgets and views.
- `-DVTK_MODULE_ENABLE_VTK_IOFFMPEG=NO`, `-DVTK_MODULE_ENABLE_VTK_IOOCCT=NO` — Leave out the module
  that reads and writes video through FFmpeg and the reader for STEP and IGES files through
  OpenCASCADE.
- `-DVTK_MODULE_USE_EXTERNAL_VTK_doubleconversion=ON` — This switch and every other
  `VTK_MODULE_USE_EXTERNAL_VTK_` switch set to `ON` (`eigen`, `expat`, `freetype`, `jpeg`,
  `jsoncpp`, `libproj`, `libxml2`, `lz4`, `lzma`, `netcdf`, `nlohmannjson`, `ogg`, `png`, `pugixml`, `sqlite`, `theora`, `tiff`, `zlib`) build
  against this tree's port of each library instead of VTK's bundled copy. Every one of those ports
  is in `depends`.
- `-DVTK_MODULE_USE_EXTERNAL_VTK_hdf5=OFF` — Keep VTK's bundled HDF5, whose symbols VTK renames
  (`vtkhdf5_*`). VTK is written against the HDF5 1.x API, the [hdf5](#port-hdf5) port is 2.x, and
  the renamed copy cannot clash with it in one process.
- `-DVTK_MODULE_USE_EXTERNAL_VTK_fmt=OFF` — Keep VTK's bundled fmt, an 11.x copy in a renamed
  namespace. The bundled IOSS library calls `fmt::localtime`, which the [fmt](#port-fmt) port's
  major version has removed.
- `-DVTK_MODULE_USE_EXTERNAL_VTK_gl2ps=OFF`, `-DVTK_MODULE_USE_EXTERNAL_VTK_utf8=OFF` — Keep the
  bundled gl2ps and utfcpp, although both are ports: VTK's gl2ps is renamed to `vtkgl2ps_*` and
  draws through VTK's own GL loader, and its utfcpp sits in a renamed namespace, so neither can
  clash with the port's library in one process.
- `-DVTK_MODULE_USE_EXTERNAL_VTK_libharu=OFF` — Keep the bundled libHaru, which is VTK's own fork.
- `-DVTK_MODULE_USE_EXTERNAL_VTK_cgns=OFF`, `-DVTK_MODULE_USE_EXTERNAL_VTK_exprtk=OFF`,
  `-DVTK_MODULE_USE_EXTERNAL_VTK_fast_float=OFF`, `-DVTK_MODULE_USE_EXTERNAL_VTK_ioss=OFF`,
  `-DVTK_MODULE_USE_EXTERNAL_VTK_pegtl=OFF`, `-DVTK_MODULE_USE_EXTERNAL_VTK_token=OFF`,
  `-DVTK_MODULE_USE_EXTERNAL_VTK_verdict=OFF` — Keep the bundled copies of libraries that are not
  ports.

## Patches

- `fix-findexpat.patch` — From Debian bug 1050506, which its `Patch-Source:` header names. It adds `NO_CACHE` to the `find_path` and `find_library` calls in VTK's own `CMake/FindEXPAT.cmake`. CMake's FindEXPAT keeps `EXPAT_LIBRARY` as an ordinary variable, and VTK's copy writing the same names into the cache conflicts with it: a project that finds expat before VTK stops with `Could NOT find EXPAT (missing: EXPAT_LIBRARY)`. Uncached, VTK's lookups set ordinary variables too.
