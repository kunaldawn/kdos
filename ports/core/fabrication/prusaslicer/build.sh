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


# Eigen 5 fails the exact 3.3.x version check and drops DynamicSparseMatrix;
# Boost 1.87-1.89 removed io_service and the header-only boost_system library;
# CGAL 6 returns optionals from its property maps; OpenCASCADE 7.8 merged the
# STEP toolkits into TKDESTEP. The OCCT wrapper goes to the library directory
# and is dlopen()ed by name. The imgui patch creates the frame the hint
# notification measures, which is otherwise a crash on wx 3.2 at start-up.
for p in eigen5 boost-1.87 boost-1.88 boost-1.89 cgal6 \
	opencascade-tkdestep occtwrapper-libdir imgui-hint-segfault; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

# Three small libraries PrusaSlicer pins and no other port uses are built
# static into a private prefix that never reaches the package: libbgcode (the
# binary G-code format, at the commit upstream's dependency build names), the
# heatshrink compressor under it, using the build files upstream ships for
# it, and fltk's nanosvg fork, whose nsvgRasterizeXY() the texture loader
# calls. Upstream's own dependency build downloads each of them instead.
_deps="$SRC_ROOT/deps"
_depopts="-G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release
	-DCMAKE_INSTALL_PREFIX=$_deps -DCMAKE_INSTALL_LIBDIR=lib
	-DCMAKE_PREFIX_PATH=$_deps -DBUILD_SHARED_LIBS=OFF
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON"

cp deps/+heatshrink/CMakeLists.txt deps/+heatshrink/Config.cmake.in \
	"$SRC_ROOT/heatshrink-$_heatshrink/"
cmake -S "$SRC_ROOT/heatshrink-$_heatshrink" -B "$SRC_ROOT/build-heatshrink" $_depopts
ninja -C "$SRC_ROOT/build-heatshrink" install

cmake -S "$SRC_ROOT/libbgcode-$_bgcode" -B "$SRC_ROOT/build-bgcode" $_depopts \
	-DLibBGCode_BUILD_TESTS=OFF \
	-DLibBGCode_BUILD_CMD_TOOL=OFF \
	-DLibBGCode_BUILD_DEPS=OFF
ninja -C "$SRC_ROOT/build-bgcode" install

cmake -S "$SRC_ROOT/nanosvg-$_nanosvg" -B "$SRC_ROOT/build-nanosvg" $_depopts
ninja -C "$SRC_ROOT/build-nanosvg" install

# Every other dependency is the system's. OpenVDB is located through the find
# module its own port installs, which knows its Imath and Blosc. FHS puts the
# resources, vendor profiles, desktop entries and hicolor PNGs under /usr/share.
# PrusaSlicer sets GDK_BACKEND=x11 for itself, because its GL canvas is GLX:
# it runs under Xwayland. The configuration and version checks run at start-up
# and find no network, which the program reports and carries on from; the
# bundled profiles are complete without them.
cmake -S . -B build -G Ninja -Wno-dev \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_PREFIX_PATH="$_deps" \
	-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON \
	-DOPENVDB_FIND_MODULE_PATH=/usr/lib/cmake/OpenVDB \
	-DSLIC3R_STATIC=OFF \
	-DSLIC3R_FHS=ON \
	-DSLIC3R_GTK=3 \
	-DSLIC3R_GUI=ON \
	-DSLIC3R_PCH=OFF \
	-DSLIC3R_ENABLE_FORMAT_STEP=ON \
	-DSLIC3R_BUILD_TESTS=OFF \
	-DSLIC3R_BUILD_SANDBOXES=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

# Bundled data is English only; the interface falls back to its source
# strings.
rm -rf "$PKG/usr/share/PrusaSlicer/localization"

# Open With matches an entry's MimeType against the canonical type the glob
# table names, and resolves no alias: upstream's entry claims .3mf and .obj
# by their aliases (application/vnd.ms-3mfdocument,
# application/prs.wavefront-obj), which would leave PrusaSlicer off the list
# for its own project format. The entry claims model/3mf and model/obj.
cat > "$PKG/usr/share/applications/PrusaSlicer.desktop" <<'EOF'
[Desktop Entry]
Name=PrusaSlicer
GenericName=3D Printing Software
Icon=PrusaSlicer
Exec=prusa-slicer %F
Terminal=false
Type=Application
MimeType=model/stl;model/3mf;model/obj;application/x-amf;
Categories=Graphics;3DGraphics;Engineering;
Keywords=3D;Printing;Slicer;slice;3D;printer;convert;gcode;stl;obj;amf;SLA
StartupNotify=false
StartupWMClass=prusa-slicer
EOF

test -x "$PKG/usr/bin/prusa-slicer"
test -e "$PKG/usr/lib/OCCTWrapper.so"
test -f "$PKG/usr/share/applications/PrusaSlicer.desktop"
test -f "$PKG/usr/share/icons/hicolor/128x128/apps/PrusaSlicer.png"
test -d "$PKG/usr/share/PrusaSlicer/profiles"
