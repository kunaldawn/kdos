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

export CXXFLAGS="$CXXFLAGS -include algorithm"
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DINSTALL_UDEV_RULES=OFF \
	-DBUILD_RFNM_UTILS=ON \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install
rm "$PKG/usr/lib/librfnm.a"

test -f "$PKG/usr/lib/librfnm.so"
test -f "$PKG/usr/include/librfnm/librfnm.h"
test -f "$PKG/usr/lib/pkgconfig/librfnm.pc"
