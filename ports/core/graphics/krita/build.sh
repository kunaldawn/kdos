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

patch -p1 -i "$PORT_SRC/0001-fftw-use-pkgconfig.patch"
patch -p1 -i "$PORT_SRC/sip-abi.patch"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_WITH_QT6=ON \
	-D ALLOW_UNSTABLE=QT6 \
	-D ENABLE_UPDATERS=OFF \
	-D FOUNDATION_BUILD=OFF \
	-D KRITA_ENABLE_PCH=OFF \
	-D BUILD_KRITA_QT_DESIGNER_PLUGINS=OFF \
	-D USE_EXTERNAL_RAQM=OFF \
	-D KRITA_QT_HAS_ENTER_LEAVE_PATCH=OFF \
	-D KRITA_QT_HAS_UNBALANCED_KEY_PRESS_RELEASE_PATCH=OFF \
	-D KRITA_QT_HAS_UPDATE_COMPRESSION_PATCH=OFF \
	-D KRITA_USE_SURFACE_COLOR_MANAGEMENT_API=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WaylandClient=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Quick=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6QuickWidgets=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6QuickControls2=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6DBus=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_PythonLibrary=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_SIP=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_PyQt6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_GSL=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KSeExpr=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_OpenEXR=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_TIFF=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_JPEG=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_GIF=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_HEIF=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_OpenJPEG=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_JPEGXL=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_FFTW3=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_OpenColorIO=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Mlt7=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibMyPaint=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Poppler=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KDcrawQt6=ON \
	-Wno-dev
grep -q '^build cmake_object_order_depends_target_kritawebpimport:' build/build.ninja
grep -qx '#define HAVE_JPEG_TURBO 1' build/config-jpeg.h
cmake --build build
DESTDIR=$PKG cmake --install build

rm -f "$PKG"/usr/share/applications/krita_*.desktop

cat > "$PKG/usr/share/applications/org.kde.krita.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Krita
GenericName=Digital Painting
Comment=Paint, illustrate and animate
Exec=krita %F
Icon=krita
Terminal=false
StartupNotify=true
StartupWMClass=org.kde.krita
MimeType=application/x-krita;
Categories=Qt;KDE;Graphics;2DGraphics;RasterGraphics;
Keywords=paint;painting;drawing;illustration;sketch;animation;brush;tablet;
EOF
chmod 644 "$PKG/usr/share/applications/org.kde.krita.desktop"
