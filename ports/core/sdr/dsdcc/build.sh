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

# The library decodes AMBE and IMBE voice frames in software with mbelib.
# dsdccx, the command-line decoder, can hand them to an AMBE dongle through
# SerialDV instead. SerialDV is named outright: the bundled find module looks
# for its header outside the serialdv/ directory it installs to, and for a
# library under a variable it never sets, so it would never find it.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DUSE_MBELIB=ON \
	-DLIBSERIALDV_INCLUDE_DIR=/usr/include/serialdv \
	-DLIBSERIALDV_LIBRARY=/usr/lib/libserialdv.so \
	-DBUILD_TOOL=ON \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

test -x "$PKG/usr/bin/dsdccx"
test -f "$PKG/usr/lib/pkgconfig/libdsdcc.pc"
readelf -d "$PKG/usr/bin/dsdccx" | grep -q libserialdv
