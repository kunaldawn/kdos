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

# Pinned to the last revision with the librfnm class API: SDR++'s RFNM source
# is written against it, and the rfnm::device API that replaced it is not
# source-compatible.
#
# INSTALL_UDEV_RULES=OFF because rfnm.rules opens the device to every user
# and names plugdev, a group this system does not have. The static archive is
# built unconditionally, so it is removed rather than shipped. rfnm_info lists
# the boards on the bus.
#
# librfnm.cpp calls std::transform without including <algorithm>, which
# libstdc++ no longer pulls in through its other headers.
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
