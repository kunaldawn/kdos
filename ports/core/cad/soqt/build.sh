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

# Qt 5 is off, so a Qt 6 that is not found fails configure (at the Qt 4
# fallback) instead of producing a Qt 5 SoQt that no Qt 6 consumer can load.
# X11 (Xext, Xi) is linked when found, for the Space Navigator input path
# under Xwayland.
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_TESTING=OFF \
	-DSOQT_BUILD_SHARED_LIBS=ON \
	-DSOQT_USE_QT6=ON \
	-DSOQT_USE_QT5=OFF \
	-DSOQT_BUILD_TESTS=OFF \
	-DSOQT_BUILD_DOCUMENTATION=OFF \
	-DHAVE_SPACENAV_SUPPORT=ON
ninja
DESTDIR=$PKG ninja install
rm -rf "$PKG/usr/share/doc"
