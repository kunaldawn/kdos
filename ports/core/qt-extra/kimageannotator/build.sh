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

# BUILD_WITH_QT6 picks the Qt 6 library, installed as kImageAnnotator-Qt6.
# libX11 is linked unconditionally on Linux: the text tool reads the Caps
# Lock state through XKB, which answers under Xwayland only.
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_SHARED_LIBS=ON \
	-D BUILD_WITH_QT6=ON \
	-D BUILD_EXAMPLE=OFF \
	-D BUILD_TESTS=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# The catalogues are built unconditionally; bundled data is English only, and
# Qt falls back to the source strings.
rm -rf "$PKG/usr/share/kImageAnnotator/translations"
