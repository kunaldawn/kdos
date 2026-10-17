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

cmake -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DENABLE_TLS=ON \
	-DENABLE_MULTITHREADING=ON \
	-DENABLE_UTILS=ON \
	-DENABLE_DOCUMENTATION=OFF \
	-DENABLE_EXAMPLES=OFF \
	-DENABLE_TESTS=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

test -e "$PKG"/usr/lib/pkgconfig/libnfs.pc
test -x "$PKG"/usr/bin/nfs-ls
