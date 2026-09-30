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

# TIFF and WebP come from the system libraries, never the bundled copies. MNG
# and JPEG 2000 are off: neither libmng nor jasper is a port, and each probe
# would otherwise decide silently.
mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DQT_BUILD_EXAMPLES=OFF \
	-DQT_BUILD_TESTS=OFF \
	-DFEATURE_tiff=ON \
	-DFEATURE_system_tiff=ON \
	-DFEATURE_webp=ON \
	-DFEATURE_system_webp=ON \
	-DFEATURE_mng=OFF \
	-DFEATURE_jasper=OFF
ninja
DESTDIR=$PKG ninja install
