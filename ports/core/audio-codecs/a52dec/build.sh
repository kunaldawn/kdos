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

# Upstream builds a static library alone unless told otherwise. The a52dec
# tool writes WAV and raw output to files; its OSS output has no device here.
./configure --prefix=/usr --libdir=/usr/lib \
	--enable-shared --disable-static --with-pic \
	--disable-oss --disable-djbfft
make
make DESTDIR=$PKG install
