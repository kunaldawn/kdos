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

# The library and the CLI only. The LADSPA, LV2 and Vamp plugins need
# ladspa.h, lv2.h and the Vamp SDK, and JNI needs a JDK; none is a port, and
# each is pinned off so a header that appears in the build root later cannot
# change what this package ships.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dfft=fftw -Dresampler=libsamplerate -Dtests=disabled \
	-Dcmdline=enabled \
	-Dladspa=disabled -Dlv2=disabled -Dvamp=disabled -Djni=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
