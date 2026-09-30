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

# The GTK 3 flavour with the IDO menu widgets; ayatana-indicator-loader is a
# development viewer and is not built.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DFLAVOUR_GTK2=OFF \
	-DFLAVOUR_GTK3=ON \
	-DENABLE_IDO=ON \
	-DENABLE_LOADER=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_COVERAGE=OFF \
	-DENABLE_WERROR=OFF
ninja
DESTDIR=$PKG ninja install
