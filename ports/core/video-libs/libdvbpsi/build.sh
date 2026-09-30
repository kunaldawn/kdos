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

# The MPEG-TS PSI/SI table decoder VLC's ts demuxer is built on: without it
# VLC plays no .ts, .m2ts or DVB stream. --enable-release drops DVBPSI_DIST,
# the development build's extra table dumps.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-release \
	--disable-debug \
	--disable-gcc-sanitize
make
make DESTDIR=$PKG install
