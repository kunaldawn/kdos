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

# QT_MAJOR_VERSION selects Qt 6; left unset, the build takes Qt 5 and names
# every library, header directory and .pc file polkit-qt5-1.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D QT_MAJOR_VERSION=6 \
	-D BUILD_EXAMPLES=OFF \
	-D BUILD_TEST=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
