## About
[IfcOpenShell]{.idx} reads and writes IFC, the open file format of building information modelling
(BIM). The port builds its C++ parser (`IfcParse`), its geometry library (`IfcGeom`, on
[OpenCASCADE](#port-opencascade)), the `IfcConvert` command, which turns an IFC model into OBJ,
glTF, STEP, IGES, SVG, HDF5, XML or another IFC, and the `ifcopenshell` Python module.
[FreeCAD](#port-freecad)'s BIM workbench imports that module to open and edit IFC files natively.

## Flags
- `-S cmake` — IfcOpenShell keeps its top-level `CMakeLists.txt` in the `cmake/` directory, not at
  the root of the source.
- `-DCMAKE_INSTALL_INCLUDEDIR:PATH=include/ifcopenshell` — Install the headers under
  `include/ifcopenshell`. Upstream installs top-level directories named `ifcparse`, `ifcgeom` and
  `serializers`, which would otherwise sit directly in `/usr/include`.
- `-DBUILD_SHARED_LIBS=ON` — Build `IfcParse` and `IfcGeom` as shared libraries; upstream's
  default is static.
- `-DVERSION_OVERRIDE=ON`, `-DADD_COMMIT_SHA=OFF` — Take the version from the `VERSION` file at
  the root of the source. Without it the libraries are versioned `0.8.0`, upstream's hardcoded
  value, whatever the release. `ADD_COMMIT_SHA` would append the git commit and branch, and needs
  a git checkout the tarball is not.
- `-DUSE_CCACHE=OFF` — Do not launch the compiler through ccache, which upstream does by default
  whenever it is on `PATH`.
- `-DBUILD_ONLY_COMMON_SCHEMAS=ON` — Compile only the schemas upstream calls common, IFC2X3,
  IFC4 and IFC4X3_ADD2, instead of every schema in its default list. Each schema is several
  megabytes of generated C++ source and header to compile.
- `-DBUILD_IFCGEOM=ON`, `-DBUILD_IFCPYTHON=ON`, `-DBUILD_CONVERT=ON`, `-DWITH_OPENCASCADE=ON` —
  Build the geometry library, the Python module and `IfcConvert`, with OpenCASCADE as the
  geometry kernel. These are upstream's defaults, stated.
- `-DBUILD_GEOMSERVER=OFF` — Build no `IfcGeomServer`, a program that exposes the IfcOpenShell
  API through a command interface on its standard input; no port here runs it.
- `-DBUILD_QTVIEWER=OFF`, `-DBUILD_EXAMPLES=OFF`, `-DBUILD_DOCUMENTATION=OFF` — Build neither
  upstream's Qt viewer, nor the examples, nor the documentation.
- `-DWITH_CGAL=OFF` — Do not build the CGAL geometry kernel. With CGAL and the Python module both
  on, the build also compiles `svgfill`, which needs the `svgpp` library from a git submodule
  that the release tarball carries empty. [cgal](#port-cgal) is in the tree; OpenCASCADE is the
  one kernel.
- `-DCOLLADA_SUPPORT=OFF`, `-DUSD_SUPPORT=OFF`, `-DWITH_ROCKSDB=OFF` — Build `IfcConvert`
  without COLLADA output (OpenCOLLADA) or USD output (Pixar's USD), and the parser without a
  RocksDB file backend. None of the three libraries is a port.
- `-DGLTF_SUPPORT=ON` — Build `IfcConvert`'s glTF output, which uses
  [nlohmann-json](#port-nlohmann-json). It is off by default.
- `-DHDF5_SUPPORT=ON` — Build HDF5 support, the `.h5` output of `IfcConvert` among it, on
  [hdf5](#port-hdf5); see `hdf5-shared.patch`. It is upstream's default.
- `-DIFCXML_SUPPORT=ON` — Read ifcXML, IFC's XML encoding, through [libxml2](#port-libxml2).
- `-DWITH_PROJ=OFF` — Build without [PROJ](#port-proj), which adds only Earth-Centered,
  Earth-Fixed glTF output.
- `-DUSE_MMAP=OFF` — Build without the command-line option to parse IFC files from
  memory-mapped files through Boost.Iostreams.
- `-DPYTHON_EXECUTABLE=/usr/bin/python3`, `-DPython_EXECUTABLE=/usr/bin/python3` — The
  interpreter the Python module is built for and the schema generator runs on. The project's
  CMake files read `PYTHON_EXECUTABLE`, and the wrapper's copies it into `Python_EXECUTABLE`, the
  variable `find_package(Python)` reads; both are given.

## Patches
- `python-version.patch` — Sets `__version__` in `ifcopenshell/__init__.py` to the release version in place of the `0.0.0` that upstream's release script rewrites. FreeCAD's BIM workbench reads `ifcopenshell.version` and, below 0.7, offers to download IfcOpenShell from the network instead of using this one. The patch names the version literally, so a version bump must edit it.
- `swig-4.5.patch` — Lets the SWIG wrapper compile with this tree's SWIG. SWIG 4.5 removed the Python 2 compatibility macros the typemaps call, so `PyInt_AsLong`, `PyInt_FromLong` and `SWIG_Python_str_FromChar` become `PyLong_AsLong`, `PyLong_FromLong` and `PyUnicode_FromString`. It also constrains `OpaqueCoordinate`'s variadic constructor to exactly N arguments convertible to `OpaqueNumber*`: SWIG returns an `OpaqueCoordinate` through a value wrapper, which the unconstrained constructor would take as one coordinate component and fail to compile, and with the constraint the copy constructor is chosen.
- `boost-optional.patch` — Adds a `profile_point` constructor that takes the radius as a plain `double`. With the tree's Boost, `boost::optional`'s converting constructor is explicit, so the `{{x, y}, {radius}}` copy-list-initialization in the profile mappings cannot use it.
- `hdf5-shared.patch` — Makes `cmake/FindHDF5.cmake` link whichever HDF5 C++ target the package exports (`hdf5_cpp-static`, `hdf5_cpp-shared` or `hdf5::hdf5_cpp-shared`) instead of always `hdf5_cpp-static`; the [hdf5](#port-hdf5) port exports only shared targets.
