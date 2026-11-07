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
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_SHARED_LIBS=ON \
	-D BUILD_TESTING=OFF \
	-D USE_QT_VERSION=6 \
	-D QCORO_BUILD_EXAMPLES=OFF \
	-D QCORO_WITH_QTDBUS=ON \
	-D QCORO_WITH_QTNETWORK=ON \
	-D QCORO_WITH_QTWEBSOCKETS=ON \
	-D QCORO_WITH_QTQUICK=ON \
	-D QCORO_WITH_QML=ON \
	-D QCORO_WITH_QTTEST=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
