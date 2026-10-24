## About

PrusaSlicer turns 3D models into G-code for FDM printers and into layer images for resin (SLA)
printers, and ships Prusa Research's and other vendors' printer profiles. Its interface is
wxWidgets on GTK 3, against the tree's [wxwidgets](#port-wxwidgets). Unlike
[orcaslicer](#port-orcaslicer), it builds against the tree's own Boost, CGAL, OpenCASCADE, OpenVDB,
oneTBB and Eigen, so most of its patches carry its code forward to the newer versions of those
libraries that the tree has.

## Flags

- `-DCMAKE_INSTALL_PREFIX=$_deps` — With the rest of `_depopts`: the three small libraries built
  first (see Notes) are configured static (`-DBUILD_SHARED_LIBS=OFF`), position-independent, and
  installed into the private prefix `$SRC_ROOT/deps`, which never reaches the package.
- `-DLibBGCode_BUILD_TESTS=OFF` — With `-DLibBGCode_BUILD_CMD_TOOL=OFF`: libbgcode without its unit
  tests or its `bgcode` command-line tool.
- `-DLibBGCode_BUILD_DEPS=OFF` — libbgcode does not run its own dependency build, which would
  download heatshrink; it finds the heatshrink built just before it in the prefix.
- `-isystem $_deps/include` — Added to `CXXFLAGS`. The sources include `<nanosvg/nanosvg.h>`, and
  NanoSVG's CMake package exports the `nanosvg/` directory itself as its include directory. Targets
  that link libbgcode get the prefix's `include/` from it; `libslic3r_cgal`, which links none of
  the private libraries, does not. Giving the prefix's `include/` to every target covers both.
- `-DCMAKE_PREFIX_PATH="$_deps"` — Finds the private prefix ahead of `/usr`.
- `-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON` — Has every `find_package` try the library's own CMake
  package before a `Find` module. Upstream sets it only for its static build.
- `-DOPENVDB_FIND_MODULE_PATH=/usr/lib/cmake/OpenVDB` — PrusaSlicer's bundled
  `FindOpenVDB.cmake` first tries the `FindOpenVDB.cmake` in this directory, the one the
  [openvdb](#port-openvdb) port installs, and falls back to its own search only when that one
  finds nothing. Configure stops with a fatal error when neither finds OpenVDB.
- `-DSLIC3R_STATIC=OFF` — Links the tree's libraries dynamically; only the three private
  libraries in `$_deps` are static. Upstream's default on Linux, named.
- `-DSLIC3R_FHS=ON` — Installs into a standard FHS layout: resources, vendor profiles, desktop
  entries and hicolor icons under `/usr/share`.
- `-DSLIC3R_GTK=3` — With `-DSLIC3R_GUI=ON`: the GUI, on GTK 3.
- `-DSLIC3R_PCH=OFF` — No precompiled headers; upstream's default is on.
- `-DSLIC3R_ENABLE_FORMAT_STEP=ON` — STEP import, through the `OCCTWrapper.so` module built on
  [opencascade](#port-opencascade). Upstream's default, named.
- `-DSLIC3R_BUILD_TESTS=OFF` — No unit tests; upstream's default is on.
- `-DSLIC3R_BUILD_SANDBOXES=OFF` — No developer sandboxes. Upstream's default, named.

## Patches

- `boost-1.87.patch` — Replaces `boost::asio::io_service`, which Boost 1.87 removed, with `io_context`, and its `post()` member with `boost::asio::post` on the executor, in the firmware dialog, the Bonjour client, the serial port and the TCP console.
- `boost-1.88.patch` — Includes Boost.Process's version 1 headers explicitly (`BOOST_PROCESS_VERSION 1`, `<boost/process/v1/...>`) in the G-code post-processor and the removable-drive manager, since `<boost/process.hpp>` names version 2 from Boost 1.88.
- `boost-1.89.patch` — Drops `system` from the Boost components the build asks for, because the header-only `boost_system` has no library to find, and includes `<boost/asio/deadline_timer.hpp>` directly in the Bonjour client, which uses the timer.
- `cgal6.patch` — CGAL 6 returns `std::optional` from a surface mesh's `property_map()` where CGAL 5 returned a pair; the patch takes `.value()` instead of `.first`, and renames `CGAL::AABB_traits` to `AABB_traits_3`, in the cut-surface code and its test.
- `eigen5.patch` — Eigen 5 fails the `find_package(Eigen3 3.3.7)` version check and drops `DynamicSparseMatrix`; the patch asks for Eigen3 with no version and rewrites the bundled libigl's `diag`, `slice` and `slice_into` with an uncompressed `SparseMatrix`. Its `igl::slice` hunk assigns the result to the const input `X` instead of the output `Y`; it compiles only because PrusaSlicer uses libigl header-only and nothing in it instantiates that template.
- `opencascade-tkdestep.patch` — OpenCASCADE 7.8 merged the STEP toolkits (`TKXDESTEP`, `TKSTEP`, `TKSTEP209`, `TKSTEPAttr`, `TKSTEPBase`) into `TKDESTEP`; the patch links that one, asks for OpenCASCADE with no version, and installs `OCCTWrapper` into the library directory.
- `serial-unistd.patch` — `src/slic3r/Utils/Serial.cpp` includes `<sys/unistd.h>`, a name only glibc carries; the patch includes `<unistd.h>`.

## Notes

Three small libraries that PrusaSlicer pins and no other port uses are vendored as extra sources
and built before the program: libbgcode, the binary G-code format, at the commit `_bgcode` that
upstream's `deps/+LibBGCode` names; heatshrink, the compressor under it, built with the
`CMakeLists.txt` and `Config.cmake.in` upstream ships for it in `deps/+heatshrink`, which
`build.sh` copies into its source; and fltk's NanoSVG fork, whose `nsvgRasterizeXY()` the texture
loader calls. Upstream's dependency build downloads each of them; here each is a `source` checked
against its `sha256` in `kpkgbuild`. Every other dependency is the system's.

`opencascade-tkdestep.patch` and `occtwrapper-libdir.patch` together put `OCCTWrapper.so` in
`/usr/lib` and have STEP import `dlopen()` it by name. The `test` lines at the end of `build.sh`
stop the build if the binary, `OCCTWrapper.so`, the desktop entry, the 128-pixel icon or the
vendor profiles are missing from the package.

PrusaSlicer sets `GDK_BACKEND=x11` for itself at start-up, because upstream's wxGTK path has no
Wayland support (upstream's comment in `src/CLI/Setup.cpp` says it crashes at start-up
otherwise), so it runs under Xwayland.

`build.sh` removes `/usr/share/PrusaSlicer/localization`. Its catalogs sit at
`<lang>/PrusaSlicer.mo`, outside any `LC_MESSAGES` directory and with no language in the file
name, so the package-wide English-only rule (see
[Size and language policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
would keep them; without them the interface shows its English source strings.

Upstream's desktop entry claims `.3mf` and `.obj` files by their aliases
(`application/vnd.ms-3mfdocument`, `application/prs.wavefront-obj`). Open With matches an entry's
`MimeType` against the canonical type the glob table names and resolves no alias, so the entry
`build.sh` writes claims `model/3mf` and `model/obj`.
