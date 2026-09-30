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

# The output to mirror is a required argument (`wl-mirror HDMI-A-1`), so the
# port ships no menu entry. wl-present is left out: it drives wl-mirror
# through pipectl, which no port installs.
# The wlr protocols come from the release tarball, which carries them; no port
# installs wlr-protocols.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DINSTALL_DOCUMENTATION=ON \
	-DINSTALL_EXAMPLE_SCRIPTS=OFF \
	-DWITH_LIBDECOR=ON \
	-DWITH_GBM=ON \
	-DFORCE_SYSTEM_WL_PROTOCOLS=ON \
	-DFORCE_SYSTEM_WLR_PROTOCOLS=OFF
ninja
DESTDIR=$PKG ninja install
