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

# The library alone, from the mt32emu/ directory of the munt tree: the Qt
# player, the ALSA MIDI daemon and the MIDI-to-WAV tool are separate programs.
# Shared, with both the C and C++ interfaces. The ROMs it needs are Roland's
# and are not shipped.
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
