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


# OpenEXR's exrcore uses this as its DEFLATE backend and finds it through
# pkg-config; without it OpenEXR's cmake reaches for the network, which the
# build does not have. The gzip program is left out because toybox already
# provides one, and nothing here links the static archive.
mkdir -p build && cd build
cmake .. \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DLIBDEFLATE_BUILD_SHARED_LIB=ON \
	-DLIBDEFLATE_BUILD_STATIC_LIB=OFF \
	-DLIBDEFLATE_BUILD_GZIP=OFF \
	-DLIBDEFLATE_BUILD_TESTS=OFF
make
make DESTDIR=$PKG install
