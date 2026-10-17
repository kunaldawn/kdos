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
cmake .. -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DRANGE_V3_TESTS=OFF \
	-DRANGE_V3_EXAMPLES=OFF \
	-DRANGE_V3_PERF=OFF \
	-DRANGE_V3_DOCS=OFF \
	-DRANGE_V3_HEADER_CHECKS=OFF \
	-DRANGES_NATIVE=OFF \
	-DRANGES_ENABLE_WERROR=OFF
make
make DESTDIR=$PKG install

rm "$PKG/usr/include/module.modulemap"
