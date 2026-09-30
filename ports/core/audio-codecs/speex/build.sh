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

# The float build with the codec's own smallft FFT. speexdsp gives speexenc
# its denoiser and libogg is what the two programs read and write; with either
# missing, configure drops that part without an error.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-binaries \
	--with-fft=smallft
make
make DESTDIR=$PKG install
