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

# The style and the colour schemes only. The KWin window decoration, the
# wallpapers and the cursor theme are off: kdos-comp draws its own frames,
# and kdos-cursors is the cursor theme. The Qt 5 style links KDE Frameworks
# 5, which is not built here.
#
# frameworkintegration is found explicitly: without it the style builds but
# loses its KStyle helpers, and nothing in the log says so.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_TESTING=OFF \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_QT5=OFF \
	-D BUILD_QT6=ON \
	-D BUILD_WITH_QTQUICK=ON \
	-D BUILD_CURSOR=OFF \
	-D WITH_DECORATIONS=OFF \
	-D WITH_WALLPAPERS=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6FrameworkIntegration=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
