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

patch -p1 -i "$PORT_SRC/sandbox-badge.patch"
patch -p1 -i "$PORT_SRC/square-corners.patch"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_TESTING=OFF \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_QT5=OFF \
	-D BUILD_QT6=ON \
	-D BUILD_WITH_QTQUICK=ON \
	-D BUILD_CURSOR=ON \
	-D WITH_DECORATIONS=ON \
	-D WITH_WALLPAPERS=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6FrameworkIntegration=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
