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


for p in eigen5 boost-1.87 boost-1.88 boost-1.89 cgal6 \
	opencascade-tkdestep occtwrapper-libdir imgui-hint-segfault serial-unistd; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

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

export CXXFLAGS="$CXXFLAGS -isystem $_deps/include"
cmake -S . -B build -G Ninja -Wno-dev \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
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

rm -rf "$PKG/usr/share/PrusaSlicer/localization"

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
