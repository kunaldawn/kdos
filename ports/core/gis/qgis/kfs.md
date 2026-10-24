## About

QGIS is a desktop geographic information system: it views, edits, analyses and prints vector,
raster, mesh and point-cloud data from files, databases and web services. The port builds QGIS
Desktop on Qt 6 with PyQGIS, the Python bindings behind the Python console, Processing, DB
Manager, MetaSearch and the plugin manager, and the command-line `qgis_process`, which runs any
Processing algorithm without the interface. GDAL, PROJ and GEOS do most of the format and
geometry work underneath it.

## Flags

- `-DBUILD_WITH_QT6=TRUE` — Builds against Qt 6 and PyQt6; upstream's default is Qt 5.
- `-DQGIS_MANUAL_SUBDIR=share/man` — Puts the manual page under `/usr/share/man`. Upstream's
  default on Unix is `man`, which under the prefix is `/usr/man`.
- `-DENABLE_TESTS=FALSE` — Builds no unit tests, which are on by default.
- `-DUSE_CCACHE=OFF` — Does not compile through ccache, which upstream uses by default when it
  finds it.
- `-DWITH_CORE=TRUE`, `-DWITH_GUI=TRUE`, `-DWITH_DESKTOP=TRUE`, `-DWITH_ANALYSIS=TRUE`,
  `-DWITH_AUTH=TRUE`, `-DWITH_OAUTH2_PLUGIN=TRUE`, `-DWITH_PYTHON=TRUE`, `-DWITH_BINDINGS=TRUE`,
  `-DWITH_QSCIAPI=TRUE`, `-DWITH_QGIS_PROCESS=TRUE`, `-DWITH_3D=TRUE` — Upstream's defaults,
  named: the core, interface and analysis libraries, the desktop program, the authentication
  methods with OAuth2, Python and its bindings, the QScintilla API file for the Python console's
  completion, `qgis_process`, and 3D map views through [qt6-qt3d](#port-qt6-qt3d).
- `-DBINDINGS_GLOBAL_INSTALL=TRUE` — Installs the `qgis` Python package into the system
  `site-packages` rather than QGIS's own directory, so `import qgis` works from any Python without
  setting `PYTHONPATH`. The `ls` after the install fails the build if `qgis._core` is not there.
- `-DWITH_QUICK=FALSE`, `-DWITH_CUSTOM_WIDGETS=FALSE`, `-DWITH_PDF4QT=FALSE` — Upstream's
  defaults, named: no QGIS Quick library for QML front ends, no Qt Designer widget plugins, and
  no embedded PDF4Qt library for PDF and HTML drawing.
- `-DWITH_SERVER=FALSE`, `-DWITH_SERVER_LANDINGPAGE_WEBAPP=FALSE` — No QGIS Server, the map
  server that runs as a FastCGI service, and so no web landing page for it; upstream's defaults.
- `-DWITH_CRASH_HANDLER=FALSE` — Leaves out the crash handler, on by default on Linux, which
  offers to send a report upstream after a crash.
- `-DWITH_GRASS7=FALSE`, `-DWITH_GRASS8=FALSE` — No GRASS GIS plugin or Processing provider;
  GRASS is not a port.
- `-DWITH_ORACLE=FALSE`, `-DWITH_HANA=FALSE` — No Oracle Spatial or SAP HANA provider; neither
  client library is a port.
- `-DWITH_POSTGRESQL=TRUE`, `-DWITH_SPATIALITE=TRUE` — The PostgreSQL/PostGIS and SpatiaLite
  providers, upstream's defaults. DB Manager reaches PostGIS through
  [python3-psycopg2](#port-python3-psycopg2).
- `-DWITH_QSPATIALITE=TRUE` — Also builds the SpatiaLite driver for Qt SQL from
  `external/qspatialite`, which is off by default. It needs Qt's private Sql headers, which
  `40-qt6.10.patch` makes the build find.
- `-DWITH_PDAL=TRUE`, `-DWITH_EPT=TRUE`, `-DWITH_COPC=TRUE`, `-DWITH_DRACO=TRUE` — Point clouds:
  the [pdal](#port-pdal) provider for LAS, LAZ and E57 files, Entwine and Cloud Optimized Point
  Cloud data, and Draco-compressed 3D tiles; upstream's defaults, named.
- `-DWITH_INTERNAL_LAZPERF=TRUE`, `-DWITH_INTERNAL_MDAL=TRUE`, `-DWITH_INTERNAL_POLY2TRI=TRUE` —
  Builds the copies QGIS carries of laz-perf (LAZ decompression for EPT and COPC), MDAL (mesh
  formats, with HDF5, NetCDF and XML support) and poly2tri (triangulation); none of the three is a
  port.
- `-DWITH_INTERNAL_MESHOPTIMIZER=FALSE` — Links the system [meshoptimizer](#port-meshoptimizer)
  rather than the copy in `external/meshOptimizer`; `system-meshoptimizer.patch` adapts the one
  call to its interface.
- `-DWITH_INTERNAL_NLOHMANN_JSON=FALSE`, `-DWITH_INTERNAL_SPATIALINDEX=FALSE` — Uses the system
  [nlohmann-json](#port-nlohmann-json) and [libspatialindex](#port-libspatialindex). The second is
  upstream's default; the first is not.
- `-DWITH_QTWEBKIT=FALSE`, `-DWITH_QTWEBENGINE=TRUE` — HTML in layouts and map tips through
  [qt6-qtwebengine](#port-qt6-qtwebengine) rather than QtWebKit, which is not a port. Upstream's
  defaults are the reverse.
- `-DWITH_QTSERIALPORT=TRUE`, `-DWITH_QTGAMEPAD=FALSE` — GPS over a serial port through
  [qt6-qtserialport](#port-qt6-qtserialport); no Qt Gamepad, which is not a port. Both are
  upstream's defaults.
- `-DWITH_GSL=TRUE` — Uses the GNU Scientific Library, upstream's default; [gsl](#port-gsl) is in
  `depends`.
- `-DUSE_OPENCL=ON` — OpenCL acceleration for some raster algorithms, through the
  [ocl-icd](#port-ocl-icd) loader; upstream's default on Linux.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_OpenCL=ON`, `-DCMAKE_REQUIRE_FIND_PACKAGE_Postgres=ON`,
  `-DCMAKE_REQUIRE_FIND_PACKAGE_HDF5=ON`, `-DCMAKE_REQUIRE_FIND_PACKAGE_NetCDF=ON`,
  `-DCMAKE_REQUIRE_FIND_PACKAGE_LibXml2=ON` — Turn five optional finds into required ones, so a
  missing library stops configure rather than building a QGIS without OpenCL, PostgreSQL, or the
  HDF5, NetCDF and XML mesh formats the MDAL provider reads (see
  [A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).

## Patches

- `10-libspatialindex_2_1.patch` — Removes the configure error QGIS raises for libspatialindex 2.1 or later. The error refers to a libspatialindex defect that the release the tree carries does not have.
- `50-qstring.patch` — Casts the snapping layer's type to `int` in `QgsSnappingUtils::dump()` before it is passed to `QString::arg()`, which with the tree's Qt does not accept the enum.
- `offline-defaults.patch` — Changes the global defaults in `resources/qgis_global_settings.ini` so that a machine with no network is not contacted at every start: the version check is off and its checkbox is hidden from the options dialog, the welcome page's news feed from `feed.qgis.org` is disabled, and the plugin manager does not check the online repository. Plugins can still be installed from a ZIP file.
- `sip-movable.patch` — Gives the PyQt6 bindings a second `std::unique_ptr` mapped type marked Movable, chosen by the `MOVABLE_MAPPED_TYPE` feature that `python/CMakeLists.txt` turns off only for a sip too old to parse the annotation. The tree's sip refuses to return a `NoRelease` mapped type by value unless it is Movable, and every binding module stops at code generation without it.
- `system-meshoptimizer.patch` — Includes the system `<meshoptimizer.h>` in `src/core/mesh/qgstriangularmesh.cpp` and passes the extra arguments its `meshopt_simplifySloppy()` takes: no vertex lock, no error target and no error result.

## Notes

The `grep` after configure checks that the OpenCL C++ header was found in `/usr/include`, from
[opencl-clhpp](#port-opencl-clhpp). Without it QGIS falls back to the `cl2.hpp` it carries in
`external/opencl-clhpp` and says so only in the configure log.

The project and layer file types QGIS defines are in upstream's RPM packaging,
`rpm/sources/qgis-mime.xml`, and not in its CMake install; `build.sh` installs that file into the
shared MIME database. Upstream's desktop entry is replaced by one that names
`StartupWMClass=org.qgis.qgis`, the Wayland app_id Qt builds from the organisation domain
`qgis.org` and the executable's name (upstream writes the X11 class `QGIS3`). It claims only those
five QGIS types, and drops the image and raster formats upstream's entry also claims, TIFF, JPEG
and JPEG 2000 among them.
