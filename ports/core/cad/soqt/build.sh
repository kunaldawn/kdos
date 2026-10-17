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

mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DSOQT_BUILD_SHARED_LIBS=ON \
	-DSOQT_USE_QT6=ON \
	-DSOQT_USE_QT5=OFF \
	-DSOQT_BUILD_TESTS=OFF \
	-DSOQT_BUILD_DOCUMENTATION=OFF \
	-DHAVE_SPACENAV_SUPPORT=ON
ninja
DESTDIR=$PKG ninja install
rm -rf "$PKG/usr/share/doc"
