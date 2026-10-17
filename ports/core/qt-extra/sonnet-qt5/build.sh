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
	-D QT_MAJOR_VERSION=5 \
	-D BUILD_TESTING=OFF \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D SONNET_USE_QML=OFF \
	-D BUILD_DESIGNERPLUGIN=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_HUNSPELL=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_ASPELL=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_HSPELL=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_VOIKKO=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
test -f "$PKG/usr/lib/cmake/KF5Sonnet/KF5SonnetConfig.cmake"
test -n "$(find "$PKG/usr/lib/qt5/plugins/kf5/sonnet" -name 'sonnet_hunspell.so')"
