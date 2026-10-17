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
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DUSE_MBELIB=ON \
	-DLIBSERIALDV_INCLUDE_DIR=/usr/include/serialdv \
	-DLIBSERIALDV_LIBRARY=/usr/lib/libserialdv.so \
	-DBUILD_TOOL=ON \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

test -x "$PKG/usr/bin/dsdccx"
test -f "$PKG/usr/lib/pkgconfig/libdsdcc.pc"
readelf -d "$PKG/usr/bin/dsdccx" | grep -q libserialdv
