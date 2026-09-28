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

# 19.7 has no PulseAudio host API: ALSA is the one backend, and PipeWire
# answers it through its ALSA plugin. JACK is not on this system and OSS is
# not a Linux sound device here.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-alsa --without-jack --without-oss --without-asihpi \
	--enable-cxx

# The library and the C++ binding race each other under a parallel make, and
# the loser fails to find a half-written object.
make -j1
make -j1 DESTDIR=$PKG install
