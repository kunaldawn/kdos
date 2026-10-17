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
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_TESTING=OFF \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_QT5=OFF \
	-D BUILD_QT6=ON \
	-D DEFAULT_UNION_STYLE=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_FontNotoSans=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_FontNotoColorEmoji=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_FontHack=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_XDGDesktopPortalKDE=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_X11=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
