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
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D PHONON_BUILD_QT5=OFF \
	-D PHONON_BUILD_QT6=ON \
	-D PHONON_BUILD_EXPERIMENTAL=ON \
	-D PHONON_BUILD_DESIGNER_PLUGIN=OFF \
	-D PHONON_BUILD_SETTINGS=OFF \
	-D PHONON_BUILD_DEMOS=OFF \
	-D PHONON_BUILD_DOC=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_PulseAudio=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_GLIB2=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
