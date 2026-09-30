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

# --with-simd=runtime compiles the resampler's convolution once per x86
# instruction set and picks one on the running processor. A fixed choice
# (sse4, avx2) builds a library that faults on any machine without it; none
# leaves the scalar loop on every machine.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-simd=runtime \
	--disable-tests
make
make DESTDIR=$PKG install
