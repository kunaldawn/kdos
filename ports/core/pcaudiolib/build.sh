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

./autogen.sh

# ALSA ONLY, AND DELIBERATELY NOT PULSE. espeak-ng has to speak on a bare tty1
# where no session bus and no pipewire exist — that is the case a screen reader
# is for — and the pulse backend would make the library prefer a server that is
# not running. On a desktop session pipewire's own ALSA compatibility layer is
# what carries it, so nothing is lost at the other end.
#
# --with-alsa does not fail when its pkg-config probe misses; alsa-lib in
# depends is what keeps the backend. OSS is off because musl ships
# sys/soundcard.h, so the probe says yes to a /dev/dsp this kernel has no
# driver behind; QSA is QNX's and off so the backend list is fixed.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-alsa --without-pulseaudio --without-oss --without-qsa
make
make DESTDIR=$PKG install
