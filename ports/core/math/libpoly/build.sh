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

cmake -B build -G Ninja \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_BUILD_TYPE=Release \
	-DLIBPOLY_BUILD_PYTHON_API=OFF \
	-DLIBPOLY_BUILD_STATIC=ON \
	-DLIBPOLY_BUILD_STATIC_PIC=ON \
	-DLIBPOLY_BUILD_STATISTICS=OFF \
	-DBUILD_TESTING=OFF
cmake --build build
DESTDIR=$PKG cmake --install build
