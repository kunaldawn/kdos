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

cmake -B build -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_CXX_STANDARD=17 \
	-DBUILD_SHARED_LIBS=ON \
	-Dwebtorrent=OFF \
	-Dgnutls=OFF \
	-Dencryption=ON \
	-Ddht=ON \
	-Dpython-bindings=ON \
	-Dpython-egg-info=ON \
	-Dpython-install-system-dir=ON \
	-Dbuild_tests=OFF \
	-Dbuild_examples=OFF \
	-Dbuild_tools=OFF
cmake --build build
DESTDIR=$PKG cmake --install build
