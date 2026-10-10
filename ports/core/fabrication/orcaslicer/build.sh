#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# OrcaSlicer is written against the dependency set its own superbuild (deps/)
# pins, and several of those sit a major version behind this tree: Boost 1.84
# (the code still uses asio's io_service, removed in 1.87), CGAL 5.6,
# OpenCASCADE 7.6, the OpenVDB 8.2 fork with its Blosc fork and OpenEXR 2.5,
# oneTBB 2021.5, NLopt 2.5, OpenCV 4.6, Draco and the libnoise fork. They are
# built here, static and private, by upstream's own superbuild: every archive
# it would download is a source of this port, laid out where ExternalProject
# looks, and each is accepted only against the SHA-256 the superbuild pins,
# so nothing is fetched. What never reaches the package: the whole prefix.
# kpkg unpacks a later tarball and keeps no copy of the archive, so each is
# linked from where kpkg found it: the port directory, or its source
# directory when the port directory does not hold it.
# ONE JOB PER 6 GiB OF MEMORY, not the build's default of one per 2 GiB: a
# compiler on OpenVDB, CGAL or libslic3r holds 3 to 4 GB, so the default on a
# 64 GB host reaches 50 GB and more, and 6 GiB leaves the rest of the host
# room. KDOS_JOBS is what script/bin/ninja hands
# every ninja call, MAKEFLAGS and CMAKE_BUILD_PARALLEL_LEVEL the rest; the
# dependencies are built one project at a time, each with that many jobs.
_mem=$(sed -n 's/^MemTotal: *\([0-9]*\) kB/\1/p' /proc/meminfo)
_jobs=$((_mem / 6291456))
[ "$_jobs" -le "$KDOS_JOBS" ] || _jobs=$KDOS_JOBS
[ "$_jobs" -ge 1 ] || _jobs=1
export KDOS_JOBS=$_jobs MAKEFLAGS=-j$_jobs CMAKE_BUILD_PARALLEL_LEVEL=$_jobs

_deps="$SRC_ROOT/orcadeps"
_dl="$SRC_ROOT/dl"
for f in "$PORT_SRC"/orcadeps-* "$SOURCE_DIR"/orcadeps-*; do
	[ -f "$f" ] || continue
	_b=${f##*/orcadeps-}
	mkdir -p "$_dl/${_b%%-*}"
	[ -e "$_dl/${_b%%-*}/${_b#*-}" ] || ln -s "$f" "$_dl/${_b%%-*}/${_b#*-}"
done
test "$(ls "$_dl"/*/* | wc -l)" -eq 14

# Two of the pinned dependencies assume glibc. oneTBB 2021.5 ORs
# RTLD_DEEPBIND, a dlopen flag musl does not have, into the flags it opens its
# own plugins with; defined as 0 it adds nothing, which is the loading musl
# does anyway. Its resumable tasks switch stacks with getcontext and
# swapcontext, which musl does not provide; __TBB_RESUMABLE_TASKS_USE_THREADS
# runs them on threads instead. Every dependency's configure reads CXXFLAGS
# from the environment, and only TBB names either. OpenCASCADE 7.6 reads heap
# usage with mallinfo, traps floating-point exceptions with feenableexcept and
# prints stack traces through <execinfo.h>; 0002-OCCT-musl.patch takes the
# branches it has for platforms without them. Boost.Filesystem 1.84 builds its
# path locale on Linux as std::locale(""), which musl's libstdc++ refuses for
# any name but C and POSIX, so the program aborts before main under the
# system's LANG=C.UTF-8; 0001-Boost-musl-locale.patch gives musl the UTF-8
# locale the BSDs get. deps-musl-patch-step has the superbuild apply both,
# the OCCT one after upstream's own.
export CXXFLAGS="$CXXFLAGS -DRTLD_DEEPBIND=0 -D__TBB_RESUMABLE_TASKS_USE_THREADS=1"
patch -p1 -i "$PORT_SRC/deps-musl-patch-step.patch"
cp "$PORT_SRC/0002-OCCT-musl.patch" deps/OCCT/
cp "$PORT_SRC/0001-Boost-musl-locale.patch" deps/Boost/

# OrcaSlicer's bundled mDNS client uses select(), fd_set and struct timeval
# and includes neither <sys/select.h> nor <sys/time.h>, which glibc's socket
# headers pull in and musl's do not; mdns-select includes them. The serial
# port code includes <sys/unistd.h>, a name only glibc carries;
# serial-unistd includes <unistd.h>.
patch -p1 -i "$PORT_SRC/mdns-select.patch"
patch -p1 -i "$PORT_SRC/serial-unistd.patch"

# FLATPAK=ON is upstream's switch for "the system provides zlib, libpng,
# expat, libjpeg, FreeType and curl": with it the superbuild builds none of
# those, and leaves wxWidgets to be built separately. Only the targets named
# below are built, with the dependencies ExternalProject orders before them.
cmake -S deps -B "$SRC_ROOT/build-deps" -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DFLATPAK=ON \
	-DDESTDIR="$_deps" \
	-DDEP_DOWNLOAD_DIR="$_dl" \
	-DDEP_WX_GTK3=ON
cmake --build "$SRC_ROOT/build-deps" --parallel 1 --target \
	dep_Boost dep_TBB dep_CGAL dep_OpenVDB dep_OCCT dep_NLopt \
	dep_libnoise dep_Draco dep_OpenCV

# wxWidgets is SoftFever's 3.3.2 fork at the head of the branch the
# superbuild clones, built static with the superbuild's options. The release
# archive carries the bundled-library submodules empty, so every image,
# compression and regex library is the system's (upstream bundles PCRE and
# libwebp), and SVG rasterising stays off as upstream has it. The GL canvas
# is EGL. OrcaSlicer includes wx's private headers, which wx does not install;
# they are copied after, as the superbuild does.
_wxsrc="$SRC_ROOT/Orca-deps-wxWidgets-$_wx"
cmake -S "$_wxsrc" -B "$SRC_ROOT/build-wx" -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX="$_deps" \
	-DCMAKE_PREFIX_PATH="$_deps" \
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
	-DwxBUILD_TOOLKIT=gtk3 \
	-DwxBUILD_SHARED=OFF \
	-DwxBUILD_PRECOMP=OFF \
	-DwxBUILD_DEBUG_LEVEL=0 \
	-DwxBUILD_SAMPLES=OFF \
	-DwxBUILD_TESTS=OFF \
	-DwxBUILD_DEMOS=OFF \
	-DwxUSE_MEDIACTRL=ON \
	-DwxUSE_DETECT_SM=OFF \
	-DwxUSE_PRIVATE_FONTS=ON \
	-DwxUSE_OPENGL=ON \
	-DwxUSE_GLCANVAS_EGL=ON \
	-DwxUSE_WEBREQUEST=ON \
	-DwxUSE_WEBVIEW=ON \
	-DwxUSE_WEBVIEW_WEBKIT=ON \
	-DwxUSE_WEBVIEW_EDGE=OFF \
	-DwxUSE_WEBVIEW_IE=OFF \
	-DwxUSE_WEBVIEW_CHROMIUM=OFF \
	-DwxUSE_LIBSDL=OFF \
	-DwxUSE_XTEST=OFF \
	-DwxUSE_STC=OFF \
	-DwxUSE_AUI=ON \
	-DwxUSE_REGEX=sys \
	-DwxUSE_ZLIB=sys \
	-DwxUSE_EXPAT=sys \
	-DwxUSE_LIBPNG=sys \
	-DwxUSE_LIBJPEG=sys \
	-DwxUSE_LIBTIFF=OFF \
	-DwxUSE_LIBWEBP=sys \
	-DwxUSE_NANOSVG=OFF \
	-DwxUSE_LUNASVG=OFF \
	-DwxUSE_LIBGNOMEVFS=OFF
ninja -C "$SRC_ROOT/build-wx" install
for d in private generic/private gtk/private; do
	mkdir -p "$_deps/include/wx-3.3/wx/$d"
	cp -r "$_wxsrc/include/wx/$d/." "$_deps/include/wx-3.3/wx/$d/"
done

# The application links the private prefix statically (SLIC3R_STATIC) and
# the system dynamically for the rest: GTK 3, WebKitGTK 4.1, GStreamer,
# libsecret, curl (SLIC3R_STATIC_EXCLUDE_CURL), OpenSSL, GLFW, GLEW, cereal,
# qhull, expat and libpng. FHS puts the resources, printer profiles, desktop
# entry and hicolor PNGs under /usr/share. Wayland is the default backend at
# run time and GDK_BACKEND=x11 opts into Xwayland. The Bambu network plugin
# is a proprietary download the program offers at run time; offline it is
# never fetched, and nothing of it is part of this build.
cmake -S . -B build -G Ninja -Wno-dev \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_PREFIX_PATH="$_deps" \
	-DwxWidgets_CONFIG_EXECUTABLE="$_deps/bin/wx-config" \
	-DSLIC3R_STATIC=ON \
	-DSLIC3R_STATIC_EXCLUDE_CURL=ON \
	-DSLIC3R_FHS=ON \
	-DSLIC3R_GTK=3 \
	-DSLIC3R_GUI=ON \
	-DSLIC3R_PCH=OFF \
	-DBBL_RELEASE_TO_PUBLIC=1 \
	-DBUILD_TESTS=OFF \
	-DORCA_TOOLS=OFF \
	-DSLIC3R_BUILD_SANDBOXES=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_Git=ON
ninja -C build
DESTDIR=$PKG ninja -C build install
rm -f "$PKG/usr/LICENSE.txt"

# PrusaSlicer's entry claims the model types, so this one claims none, and
# the orcaslicer:// handler only answers links from the online model sites.
cat > "$PKG/usr/share/applications/com.orcaslicer.OrcaSlicer.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=OrcaSlicer
GenericName=3D Printing Slicer
Comment=Slice 3D models into G-code, with printer calibration tools
Exec=orca-slicer %F
Icon=OrcaSlicer
Terminal=false
Categories=Graphics;3DGraphics;Engineering;
Keywords=3d;printing;slicer;gcode;stl;3mf;calibration;
StartupWMClass=orca-slicer
DESKTOP
chmod 644 "$PKG/usr/share/applications/com.orcaslicer.OrcaSlicer.desktop"

test -x "$PKG/usr/bin/orca-slicer"
test -d "$PKG/usr/share/OrcaSlicer/profiles"
test -f "$PKG/usr/share/icons/hicolor/128x128/apps/OrcaSlicer.png"
