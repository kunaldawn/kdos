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

patch -p1 -i "$PORT_SRC/release-tarball-version.patch"

# Qt 6 only: every consumer here (libkleo, Kleopatra) looks for QGpgmeQt6, and
# a Qt 5 build would pull qt5-qtbase into a package nothing links against it.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_WITH_QT5=OFF \
	-DBUILD_WITH_QT6=ON \
	-DBUILD_TESTING=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON
ninja
DESTDIR=$PKG ninja install
