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

# KF_SKIP_PO_PROCESSING leaves every translation out: bundled data is English
# only, and without it each language's catalogue is compiled and installed.
#
# The raw thumbnailer is built only when both libkexiv2 and libkdcraw are
# found, so both are required here rather than left to detection. The
# Mobipocket reader library is not a port. PostScript and PDF thumbnails run
# the gs binary at run time.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_TESTING=OFF \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D DISABLE_MOBIPOCKET=ON \
	-D DISABLE_BLENDER=OFF \
	-D BUILD_FUZZERS=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KExiv2Qt6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KDcrawQt6=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
