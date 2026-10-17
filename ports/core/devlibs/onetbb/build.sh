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
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DTBB_TEST=OFF \
	-DTBB_EXAMPLES=OFF \
	-DTBB_STRICT=OFF \
	-DTBB4PY_BUILD=OFF \
	-DTBBMALLOC_BUILD=ON \
	-DTBBMALLOC_PROXY_BUILD=OFF \
	-DTBB_DISABLE_HWLOC_AUTOMATIC_SEARCH=OFF
ninja
DESTDIR=$PKG ninja install
test -e "$PKG"/usr/lib/libtbbbind_2_5.so
