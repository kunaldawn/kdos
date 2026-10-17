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

mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DINSTALL_DOCUMENTATION=ON \
	-DINSTALL_EXAMPLE_SCRIPTS=OFF \
	-DWITH_LIBDECOR=ON \
	-DWITH_GBM=ON \
	-DFORCE_SYSTEM_WL_PROTOCOLS=ON \
	-DFORCE_SYSTEM_WLR_PROTOCOLS=OFF
ninja
DESTDIR=$PKG ninja install
