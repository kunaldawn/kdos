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

cmake -S mt32emu -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-Dlibmt32emu_SHARED=ON \
	-Dlibmt32emu_C_INTERFACE=ON \
	-Dlibmt32emu_CPP_INTERFACE=ON \
	-DBUILD_TESTING=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install
install -Dm644 mt32emu/COPYING.LESSER.txt "$PKG/usr/share/licenses/$name/COPYING.LESSER.txt"
install -Dm644 mt32emu/COPYING.txt "$PKG/usr/share/licenses/$name/COPYING.txt"
