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

# BUILD_TESTING=OFF: the test suite pulls GoogleTest with FetchContent, which
# reaches the network. USE_RPATH=OFF: the library lands on the linker's own
# path, and an $ORIGIN rpath in every tool would only shadow it.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_APPS=ON \
	-DBUILD_SHAPELIB_CONTRIB=ON \
	-DUSE_RPATH=OFF \
	-DBUILD_TESTING=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

test -e "$PKG/usr/lib/libshp.so"
test -e "$PKG/usr/include/shapefil.h"
