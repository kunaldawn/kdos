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

cmake -S . -B "$SRC_ROOT/build" -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DJAS_ENABLE_SHARED=ON \
	-DJAS_PACKAGING=ON \
	-DJAS_ENABLE_LIBJPEG=ON \
	-DJAS_ENABLE_LIBHEIF=OFF \
	-DJAS_ENABLE_OPENGL=OFF \
	-DJAS_ENABLE_DOC=OFF \
	-DJAS_ENABLE_LATEX=OFF \
	-DJAS_ENABLE_PROGRAMS=ON \
	-DJAS_ENABLE_MULTITHREADING_SUPPORT=ON \
	-DJAS_ENABLE_CMAKE_PACKAGE_CONFIG=ON
cmake --build "$SRC_ROOT/build"
DESTDIR=$PKG cmake --install "$SRC_ROOT/build"
