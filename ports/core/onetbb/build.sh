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


# TBB_STRICT=OFF: upstream's -Werror would turn a new compiler warning into a
# failed build. tbbmalloc_proxy replaces malloc in every process that loads
# it, and hooks glibc-internal allocator entry points musl does not have, so it
# is not built; tbbmalloc itself is. tbbbind, which pins task arenas to NUMA
# nodes and core types, is built against hwloc, found through pkg-config.
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
