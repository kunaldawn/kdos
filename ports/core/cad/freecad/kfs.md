## About
[FreeCAD]{.idx} is a parametric 3D modeller for mechanical design: parts are built from
constrained sketches and features in the Part Design and Sketcher workbenches, and other
workbenches turn the same model into drawings (TechDraw), finite-element studies (FEM), toolpaths
(CAM), buildings (BIM) and assemblies. It is C++ with many of its workbenches written in Python, on Qt 6
through PySide6, drawing its 3D view with [Coin](#port-coin) and modelling with
[OpenCASCADE](#port-opencascade).

## Flags
- `-DCMAKE_INSTALL_PREFIX=$_prefix` — install into `/usr/lib/freecad`, which is upstream's own
  default prefix on Linux. FreeCAD finds its modules relative to its executable, so the program
  tree stays together there, with the libraries in `/usr/lib/freecad/lib`
  (`CMAKE_INSTALL_LIBDIR`) and the headers in `/usr/lib/freecad/include`
  (`CMAKE_INSTALL_INCLUDEDIR`); `build.sh` links `FreeCAD`, `FreeCADCmd` and `freecad-thumbnailer`,
  and the lower-case `freecad` and `freecadcmd`, into `/usr/bin`.
- `-DCMAKE_INSTALL_BINDIR=$_prefix/bin` — the programs' directory, given as an absolute path
  because upstream writes it into the `TryExec` and `Exec` lines of the installed
  `FreeCAD.thumbnailer`, which a file manager runs to draw a `.FCStd` file's preview.
- `-DCMAKE_INSTALL_DATADIR=/usr/share/freecad` — with `CMAKE_INSTALL_DATAROOTDIR=/usr/share` and
  `CMAKE_INSTALL_DOCDIR=/usr/share/doc/freecad`, put the shared data, the desktop files and icons,
  and the documentation under `/usr/share` rather than inside the prefix. FreeCAD compiles the
  data and documentation directories into the program (`RESOURCEDIR` and `DOCDIR` in
  `ConfigureCMakeVariables.cmake`), so these absolute paths are where it looks at run time.
  `/usr/share/doc/freecad` holds the pages the About dialog shows, which is why the tree's
  [HTML prune](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy) leaves
  that directory alone.
- `-DPython3_EXECUTABLE=/usr/bin/python3` — the interpreter FreeCAD embeds and builds its modules
  for, and the one it runs at configure time to ask Pivy for its versions.
- `-Dpybind11_DIR="$(python3 -m pybind11 --cmakedir)"` — where pybind11's CMake package is. The
  [python3-pybind11](#port-python3-pybind11) port installs it inside the Python package, where
  CMake does not search on its own.
- `-DCOIN3D_VERSION="$_coin"` — with `COIN3D_MAJOR_VERSION`, `COIN3D_MINOR_VERSION` and
  `COIN3D_MICRO_VERSION`, Coin's version as `pkg-config --modversion Coin` reports it. Left unset,
  `SetupCoin3D.cmake` reads the three numbers out of `Inventor/C/basic.h` with a pattern that takes
  one digit for each, so a two-digit micro version is truncated. The configure then compares that
  with the Coin version Pivy reports (`FREECAD_CHECK_PIVY`, below) and stops on a mismatch.
- `-DFREECAD_QT_VERSION=6` — build against Qt 6. Upstream's default is `Auto`, which takes Qt 5
  when both are installed.
- `-DBUILD_GUI=ON` — the graphical program, upstream's default, named.
- `-DBUILD_TEST=OFF` — leave out the Test workbench, and with `-DENABLE_DEVELOPER_TESTS=OFF` the
  C++ unit-test suite. Both are on by default upstream.
- `-DBUILD_ADDONMGR=OFF` — leave out the Addon Manager, the workbench that downloads and updates
  third-party workbenches, macros and preference packs from FreeCAD's addon repository on GitHub.
  It is on by default upstream.
- `-DBUILD_FEM=ON` — the FEM workbench, upstream's default; `-DBUILD_FEM_NETGEN=ON` adds the
  [Netgen](#port-netgen) mesher to it, which upstream builds by default only on Windows. The FEM
  solver it drives, [calculix](#port-calculix), is a run-time dependency.
- `-DBUILD_DESIGNER_PLUGIN=OFF` — no plugin for Qt Designer; upstream's default, named.
- `-DFREECAD_USE_PCL=OFF` — leave out the point-cloud features built on the Point Cloud Library,
  which the tree does not carry. Upstream's default, named.
- `-DFREECAD_USE_3DCONNEXION_LEGACY=ON` — on Linux, 3D-mouse support through
  [libspnav](#port-libspnav), which is in `depends`; it is compiled in when the library is found.
  It is upstream's default.
- `-DFREECAD_USE_FREETYPE=ON` — compile the Part module's text-to-geometry code (`FT2FC.cpp`,
  behind `Part.makeWireString`) against FreeType; upstream's default, named.
- `-DFREECAD_USE_EXTERNAL_FMT=ON` — use the [fmt](#port-fmt) port. When it is off or the library is
  not found, upstream downloads fmt's source from GitHub during the configure, which the build's
  lack of network would turn into a failure.
- `-DFREECAD_USE_EXTERNAL_SMESH=OFF` — build the SALOME SMESH mesh library from the copy under
  `src/3rdParty/salomesmesh`, upstream's default. No SMESH port exists; the bundled copy reads MED
  files through [libmed](#port-libmed) and [hdf5](#port-hdf5).
- `-DFREECAD_USE_PYSIDE=ON` — link the PySide6 and, with `-DFREECAD_USE_SHIBOKEN=ON`, Shiboken6
  libraries at build time instead of importing their Python modules at run time. Both are
  upstream's defaults; upstream turns each off with only a warning when its headers are missing.
- `-DFREECAD_CHECK_PIVY=ON` — at configure time, import Pivy (the Python binding of Coin) and stop
  if the Coin version it was built against is not the one FreeCAD links. It is upstream's default.
- `-DAPPDATA_RELEASE_DATE=$_date` — the release date written into the AppStream metadata. Upstream
  takes it from `git log` in a checkout and from the day of the build otherwise, so without the
  switch two builds of one recipe would differ.

## Patches
- `UnlimitedUnsigned.h-cstdint.patch` — adds `#include <cstdint>` to `src/Base/UnlimitedUnsigned.h`, which uses `int32_t` without including the header that declares it. It compiles only where some other included header happens to pull `<cstdint>` in.

## Notes
The source is the release tarball rather than the tag archive, because only the tarball carries
the OndselSolver, GSL and AddonManager submodules. It has no top-level directory: its files sit
at the root of the archive. kpkg
strips one directory from the first source as it unpacks it, which would take the top of
FreeCAD's tree with it, so `build.sh` unpacks the tarball again, whole, into `unpacked/` and builds
there. `no-execinfo.patch` carries its own description: it removes the `<execinfo.h>` include,
which musl does not provide, from `src/App/Application.cpp`, and the include and the
`backtrace()` calls from the bundled SMESH's `Basics_Utils.cpp`.

The Python side is part of `depends`, not a vendor bundle: PySide6, Pivy, NumPy, Matplotlib,
PyYAML, PLY and packaging are each a port, and [ifcopenshell](#port-ifcopenshell) gives the BIM
workbench its IFC import and export.

The desktop entry `build.sh` writes runs `FreeCAD --single-instance`, so opening a file from the
file manager while FreeCAD is running hands it to the open window.
