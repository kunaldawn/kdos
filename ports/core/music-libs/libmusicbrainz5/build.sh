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

patch -p1 -i "$PORT_SRC/custom-command-inc-depends.patch"
patch -p1 -i "$PORT_SRC/libxml2-const-error.patch"
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D BUILD_SHARED_LIBS=ON
cmake --build build
DESTDIR=$PKG cmake --install build
test -e "$PKG/usr/lib/libmusicbrainz5.so.1"
test -f "$PKG/usr/lib/pkgconfig/libmusicbrainz5.pc"
test -f "$PKG/usr/include/musicbrainz5/mb5_c.h"
