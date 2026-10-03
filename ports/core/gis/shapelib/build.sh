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
# reaches the network. The library lands on the linker's own path, so no
# binary carries an rpath: USE_RPATH=OFF covers the library, and
# CMAKE_SKIP_INSTALL_RPATH the tools, which get an $ORIGIN one regardless of
# USE_RPATH. CMAKE_INSTALL_LIBDIR is left at the project's default, lib: the
# project declares it a PATH cache entry, and a relative value given on the
# command line is made absolute against the build directory, which installs
# the library into the source tree.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_APPS=ON \
	-DBUILD_SHAPELIB_CONTRIB=ON \
	-DUSE_RPATH=OFF \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DBUILD_TESTING=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

test -e "$PKG/usr/lib/libshp.so"
test -e "$PKG/usr/include/shapefil.h"
