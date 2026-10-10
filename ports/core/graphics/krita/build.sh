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

# fftw's autotools install writes an FFTW3Config.cmake that names targets it
# never defines; Krita's find module takes it first and fails. The patch makes
# the module use pkg-config only.
#
# sip-abi has the Qt 6 Python bindings target sip ABI 13 at its newest minor
# version, as PyQt6 itself does, in place of a fixed 13.0: PyQt6 declares a
# minimum ABI above that, and sip refuses to generate the krita module.
patch -p1 -i "$PORT_SRC/0001-fftw-use-pkgconfig.patch"
patch -p1 -i "$PORT_SRC/sip-abi.patch"

# BUILD_WITH_QT6 selects Qt 6 and KF6; without it the project looks for Qt 5.
# Upstream still marks the Qt 6 build unstable, and the configure stops unless
# ALLOW_UNSTABLE names QT6.
# ENABLE_UPDATERS=OFF removes the update check and its notification.
#
# The three KRITA_QT_HAS_* switches name patches Krita's own Qt carries and
# this Qt does not. Off, Krita keeps its workarounds for unbalanced
# enter/leave and key events and draws the canvas without frame compression;
# on, those events go unhandled.
#
# Wayland and X11 are both built: the Wayland platform plugin, with the
# color-management-v1 client, generates its protocol code from XML in the
# tarball; the X11 path links libXi for tablet events under Xwayland.
#
# Every feature library below is an optional find upstream, and a missing one
# builds a Krita without that file format, filter or engine. Each is required
# here so that a missing port fails the configure. WebP and libjpeg-turbo are
# the two exceptions, checked after the configure instead: a requirement also
# applies to the config-package search their find modules make first, and
# both fail it with the library present. libwebp installs no WebPConfig.cmake,
# and libjpeg-turbo's config file rejects the turbojpeg component the module
# was asked for, which it never marks found.
#   Poppler (Qt 6 bindings)  the PDF import filter
#   Mlt7 (with SDL2)         audio in animation
#   PythonLibrary, SIP, PyQt6  the Python plugin host and its scripts
#   KDcrawQt6                camera RAW import
#   KSeExpr                  the SeExpr fill layer
#   Qt6Quick*                the QML-based touch dockers
# G'MIC-Qt is a separate program built against the installed interface
# library and is not part of this port.
#
# KF_SKIP_PO_PROCESSING leaves every translation out: bundled data is English
# only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
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

# Each import filter installs a hidden entry claiming its format (image/png,
# image/jpeg and the rest), which would make Krita a second handler for every
# common picture type. The one entry below claims Krita's own document type
# only; the brush-preset type upstream names is not in the MIME database.
rm -f "$PKG"/usr/share/applications/krita_*.desktop

# QGuiApplication::setDesktopFileName makes the Wayland app_id org.kde.krita;
# under Xwayland the window class is krita.
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
