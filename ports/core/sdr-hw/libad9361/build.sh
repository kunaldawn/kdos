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

# The library, ad9361.h and libad9361.pc. The Doxygen reference and the
# package-building target stay off; the tests need a radio attached.
cmake -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_TESTS=OFF \
	-DWITH_DOC=OFF \
	-DPYTHON_BINDINGS=OFF \
	-DMATLAB_BINDINGS=OFF \
	-DENABLE_PACKAGING=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

test -e "$PKG"/usr/include/ad9361.h
test -e "$PKG"/usr/lib/pkgconfig/libad9361.pc
