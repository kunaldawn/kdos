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

# --enable-libgsl IS WHAT TURNS ON THE POLYSOMY AND ASSOCIATION PLUGINS, which
# are otherwise absent from a binary whose documentation lists them. gsl is
# already a port for the C2 ring, so it costs nothing here.
#
# It also relicenses the result: bcftools is MIT/Expat and GSL is GPL-3, so a
# bcftools linked against it is GPL-3. That is the same shape of decision as
# ffmpeg's `--enable-gpl` and is recorded rather than left implicit.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--with-htslib=system \
	--enable-libgsl
make
make DESTDIR=$PKG install
