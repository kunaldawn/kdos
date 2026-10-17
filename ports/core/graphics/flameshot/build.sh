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

mkdir -p external
mv "$SRC_ROOT/Qt-Color-Widgets-$_qcw" external/Qt-Color-Widgets

cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_DISABLE_FIND_PACKAGE_Git=ON \
	-D USE_KDSINGLEAPPLICATION=ON \
	-D USE_BUNDLED_KDSINGLEAPPLICATION=OFF \
	-D USE_WAYLAND_CLIPBOARD=ON \
	-D USE_LAUNCHER_ABSOLUTE_PATH=OFF \
	-D ENABLE_IMGUR=OFF \
	-D DISABLE_UPDATE_CHECKER=ON \
	-D USE_MONOCHROME_ICON=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

rm -rf "$PKG/usr/share/flameshot/translations"
