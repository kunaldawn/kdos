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
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_WITH_QT6=ON \
	-D BUILD_TESTS=OFF \
	-D BUILD_TOOLS=OFF \
	-D BUILD_PLUGINS="ossl;gnupg;cyrus-sasl;logger;softstore" \
	-D QCA_FEATURE_INSTALL_DIR=/usr/lib/qt6/mkspecs/features \
	-D CMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
