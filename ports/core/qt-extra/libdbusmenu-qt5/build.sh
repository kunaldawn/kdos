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

cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D USE_QT5=ON \
	-D WITH_DOC=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_QJSON=ON \
	-Wno-dev
cmake --build build --target dbusmenu-qt5
DESTDIR=$PKG cmake --install build
test -e "$PKG/usr/lib/libdbusmenu-qt5.so.2"
test -f "$PKG/usr/lib/cmake/dbusmenu-qt5/dbusmenu-qt5-config.cmake"
