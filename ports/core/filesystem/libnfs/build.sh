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

# RPC-with-TLS is asked for by name: the AUTO default builds it only when
# gnutls and the kernel's kTLS headers are both found, so its presence would
# follow build order. The tools and their pre-generated manual pages install
# together; ENABLE_DOCUMENTATION would regenerate the pages from a docbook
# stylesheet fetched over the network.
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
