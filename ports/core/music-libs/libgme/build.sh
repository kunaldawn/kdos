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

# GME_ZLIB is on so gzip-compressed VGZ and friends play. UBSan is upstream's
# default for a debug build and is off here.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DGME_BUILD_SHARED=ON \
	-DGME_BUILD_STATIC=OFF \
	-DGME_BUILD_TESTING=OFF \
	-DGME_BUILD_EXAMPLES=OFF \
	-DGME_ENABLE_UBSAN=OFF \
	-DGME_ZLIB=ON
ninja
DESTDIR=$PKG ninja install
