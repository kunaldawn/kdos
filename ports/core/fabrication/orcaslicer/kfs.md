## About

OrcaSlicer turns 3D models into G-code for FDM printers. It descends from Bambu Studio and
PrusaSlicer, adds printer calibration tools, and carries a large library of printer profiles. Its
interface is wxWidgets on GTK 3, with WebKitGTK for its built-in web pages. It is one of the
largest builds in the tree: besides the program, `build.sh` compiles a private copy of most of the
libraries OrcaSlicer is written against, through upstream's own dependency superbuild in `deps/`.

## Flags

- `KDOS_JOBS=$_jobs` — With `MAKEFLAGS=-j$_jobs` and `CMAKE_BUILD_PARALLEL_LEVEL=$_jobs`: one
  compile job per 6 GiB of memory, read from `/proc/meminfo`, never more than the tree's own
  `KDOS_JOBS` and never fewer than one. A compiler on OpenVDB, CGAL or libslic3r holds 3 to 4 GB,
  so the tree's usual count would exhaust a large host. `KDOS_JOBS` is the count the tree's
  `ninja` wrapper hands every ninja call; the other two reach `make` and `cmake --build`.
- `-DRTLD_DEEPBIND=0` — The pinned oneTBB ORs `RTLD_DEEPBIND`, a `dlopen` flag musl does not have,
  into the flags it opens its own plugins with; defined as 0 it adds nothing.
- `-D__TBB_RESUMABLE_TASKS_USE_THREADS=1` — oneTBB's resumable tasks switch stacks with
  `getcontext` and `swapcontext`, which musl does not provide; this runs them on threads instead.
  Both TBB defines go in `CXXFLAGS`, which every dependency's configure reads from the
  environment, and only TBB names either.
- `-DFLATPAK=ON` — Upstream's switch for a build where the system provides zlib, libpng, expat,
  libjpeg, FreeType, OpenSSL and curl: the superbuild finds those instead of building them, and
  leaves wxWidgets to be built separately. It also installs into `DESTDIR` itself rather than
  `DESTDIR/usr/local`, and deletes each project's download directory once it is unpacked and its
  source and build trees once it is installed. Only the superbuild is configured with it.
- `-DDESTDIR="$_deps"` — The prefix the superbuild installs every dependency into,
  `$SRC_ROOT/orcadeps`. Nothing from it reaches the package; the program links it statically.
- `-DDEP_DOWNLOAD_DIR="$_dl"` — Where ExternalProject looks for each dependency's archive, as
  `<dir>/<project>/<file>`; see Notes.
- `-DDEP_WX_GTK3=ON` — GTK 3 for the superbuild's own wxWidgets project, which `FLATPAK` keeps
  out of this build. Upstream's default, named.
- `-DwxBUILD_TOOLKIT=gtk3` — With `-DwxBUILD_SHARED=OFF`, `-DwxBUILD_DEBUG_LEVEL=0`,
  `-DwxUSE_MEDIACTRL=ON`, `-DwxUSE_DETECT_SM=OFF`, `-DwxUSE_PRIVATE_FONTS=ON`,
  `-DwxUSE_OPENGL=ON`, `-DwxUSE_WEBREQUEST=ON`, `-DwxUSE_WEBVIEW=ON`, `-DwxUSE_LIBSDL=OFF`,
  `-DwxUSE_XTEST=OFF`, `-DwxUSE_STC=OFF` and `-DwxUSE_AUI=ON`: the options upstream's
  `deps/wxWidgets/wxWidgets.cmake` builds wxWidgets with, repeated here because the separate
  wxWidgets build does not read that file.
- `-DwxBUILD_PRECOMP=OFF` — No precompiled headers; upstream's superbuild turns them on.
- `-DwxUSE_GLCANVAS_EGL=ON` — The OpenGL canvas makes its context through EGL, which works on
  Wayland; upstream sets it too.
- `-DwxUSE_WEBVIEW_WEBKIT=ON` — With `-DwxUSE_WEBVIEW_EDGE=OFF`, `-DwxUSE_WEBVIEW_IE=OFF` and
  `-DwxUSE_WEBVIEW_CHROMIUM=OFF`: the web view is [webkitgtk](#port-webkitgtk) alone.
- `-DwxUSE_REGEX=sys` — With `-DwxUSE_ZLIB=sys`, `-DwxUSE_EXPAT=sys`, `-DwxUSE_LIBPNG=sys`,
  `-DwxUSE_LIBJPEG=sys` and `-DwxUSE_LIBWEBP=sys`: every image, compression and regex library is
  the tree's. The fork's archive leaves its bundled-library submodules empty, and upstream builds
  PCRE and libwebp from them.
- `-DwxUSE_LIBTIFF=OFF` — With `-DwxUSE_NANOSVG=OFF`, `-DwxUSE_LUNASVG=OFF` and
  `-DwxUSE_LIBGNOMEVFS=OFF`: no TIFF images, no SVG rasteriser and no GNOME VFS in wxWidgets.
  Upstream turns TIFF and NanoSVG off; LunaSVG is off by wxWidgets' own default.
- `-DwxWidgets_CONFIG_EXECUTABLE="$_deps/bin/wx-config"` — Points OrcaSlicer at the private
  wxWidgets build rather than any `wx-config` on `PATH`.
- `-DSLIC3R_STATIC=ON` — Links the private prefix statically. Upstream's default, named.
- `-DSLIC3R_STATIC_EXCLUDE_CURL=ON` — Links the system [curl](#port-curl) dynamically even under
  `SLIC3R_STATIC`.
- `-DSLIC3R_FHS=ON` — Installs into a standard FHS layout: resources, printer profiles and icons
  under `/usr/share`, the binary in `/usr/bin`.
- `-DSLIC3R_GTK=3` — With `-DSLIC3R_GUI=ON`: the GUI, on GTK 3. Upstream's defaults, named.
- `-DSLIC3R_PCH=OFF` — No precompiled headers; upstream's default is on.
- `-DBBL_RELEASE_TO_PUBLIC=1` — Compiles the public-release behaviour, among it the lower log
  level of the release builds. Upstream sets it for Release builds; named here.
- `-DORCA_TOOLS=OFF` — With `-DSLIC3R_BUILD_SANDBOXES=OFF`: none of upstream's developer tools or
  sandboxes. Upstream's defaults, named.
- `-DCMAKE_DISABLE_FIND_PACKAGE_Git=ON` — The main project does not look for git. It only uses
  git to read a commit hash from a `.git` directory, which the release archive has none of.

## Patches

- `0002-OCCT-musl.patch` — Applied by the superbuild to its OpenCASCADE. OCCT reads heap usage with `mallinfo`, traps floating-point exceptions with `feenableexcept`/`fegetexcept`, and prints stack traces through `<execinfo.h>`, none of which musl has; the patch takes the branches OCCT has for platforms without them.
- `deps-musl-patch-step.patch` — Adds `0001-Boost-musl-locale.patch` and `0002-OCCT-musl.patch` to the superbuild: OCCT's `git apply` takes `0002-OCCT-musl.patch` after upstream's own `0001-OCCT-fix.patch`, and Boost gains a `PATCH_COMMAND` for `0001-Boost-musl-locale.patch`. `build.sh` copies both into `deps/OCCT/` and `deps/Boost/`, where those commands look.
- `deps-system-gmp-mpfr.patch` — Removes the superbuild's GMP and MPFR projects and the dependencies of CGAL and Eigen on them, so CGAL finds the [gmp](#port-gmp) and [mpfr](#port-mpfr) ports instead of private copies.
- `mdns-select.patch` — The bundled mDNS client in `deps_src/mdns/mdns.h` uses `select()`, `fd_set` and `struct timeval` and includes neither `<sys/select.h>` nor `<sys/time.h>`, which glibc's socket headers pull in and musl's do not; the patch includes both.
- `serial-unistd.patch` — `src/slic3r/Utils/Serial.cpp` includes `<sys/unistd.h>`, a name only glibc carries; the patch includes `<unistd.h>`.

## Notes

OrcaSlicer is written against the dependency set its superbuild pins, and most of those are
older than the tree's own ports: Boost (the code uses asio's `io_service`, which later Boost
releases remove), CGAL, OpenCASCADE, a fork of OpenVDB with its forks of Blosc and OpenEXR,
oneTBB, NLopt and OpenCV. Draco and a libnoise fork are built the same way. The versions are in
the archive names in `kpkgbuild`. All of them are vendored, built static and private, and never
packaged. Only the targets named on the `cmake --build` line are
built, with the projects ExternalProject orders before them (Eigen, Blosc, OpenEXR); `--parallel 1`
builds one project at a time, each with the job count above.

Every archive the superbuild would download is a `source` of this port, named
`orcadeps-<Project>-<file>`. `build.sh` links each into `$SRC_ROOT/dl/<Project>/<file>`, from the
port directory or from kpkg's source directory, wherever kpkg found it; ExternalProject then finds
the file in its download directory and accepts it only against the `URL_HASH SHA256` the
superbuild pins beside the URL, so nothing is fetched. `test "$(ls "$_dl"/*/* | wc -l)" -eq 12`
stops the build unless every archive is in place. A version bump of OrcaSlicer that moves
a pin needs the matching archive as a new source, or the superbuild tries the network and fails.
[git](#port-git) is in `depends` because the superbuild applies its patches with `git apply`.

wxWidgets is SoftFever's fork, which the superbuild would clone with git by tag; here it is the
archive of the commit `_wx` in `kpkgbuild`, built by hand with the superbuild's options and
installed into the same private prefix. OrcaSlicer includes wxWidgets' private headers, which
wxWidgets does not install, so `build.sh` copies `private`, `generic/private` and `gtk/private`
after the install, as the superbuild does.

The program links the system dynamically for everything else: [gtk3](#port-gtk3),
[webkitgtk](#port-webkitgtk), GStreamer, libsecret, curl, OpenSSL, GLFW, GLEW, cereal, qhull,
expat and libpng. The three `test` lines at the end stop the build if the binary, the printer
profiles or the 128-pixel icon are missing from the package. Upstream installs a `LICENSE.txt`
directly under `/usr`, which `build.sh` removes.

The Bambu network plugin is a proprietary download the program offers at run time; nothing of it
is part of this build.
