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


_mem=$(sed -n 's/^MemTotal: *\([0-9]*\) kB/\1/p' /proc/meminfo)
_jobs=$((_mem / 6291456))
[ "$_jobs" -le "$KDOS_JOBS" ] || _jobs=$KDOS_JOBS
[ "$_jobs" -ge 1 ] || _jobs=1
export KDOS_JOBS=$_jobs MAKEFLAGS=-j$_jobs CMAKE_BUILD_PARALLEL_LEVEL=$_jobs
unset KDOS_JOBSERVER GNUMAKEFLAGS CARGO_MAKEFLAGS

_deps="$SRC_ROOT/orcadeps"
_dl="$SRC_ROOT/dl"
for f in "$PORT_SRC"/orcadeps-* "$SOURCE_DIR"/orcadeps-*; do
	[ -f "$f" ] || continue
	_b=${f##*/orcadeps-}
	mkdir -p "$_dl/${_b%%-*}"
	[ -e "$_dl/${_b%%-*}/${_b#*-}" ] || ln -s "$f" "$_dl/${_b%%-*}/${_b#*-}"
done
test "$(ls "$_dl"/*/* | wc -l)" -eq 12

export CXXFLAGS="$CXXFLAGS -DRTLD_DEEPBIND=0 -D__TBB_RESUMABLE_TASKS_USE_THREADS=1"
patch -p1 -i "$PORT_SRC/deps-musl-patch-step.patch"
patch -p1 -i "$PORT_SRC/deps-system-gmp-mpfr.patch"
cp "$PORT_SRC/0002-OCCT-musl.patch" deps/OCCT/
cp "$PORT_SRC/0001-Boost-musl-locale.patch" deps/Boost/

patch -p1 -i "$PORT_SRC/mdns-select.patch"
patch -p1 -i "$PORT_SRC/serial-unistd.patch"

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
