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
#
# --with-cblas=openblas: the CBLAS under gsl is OpenBLAS on this tree (see the
# gsl recipe), and naming it links bcftools to it directly. The default search
# tries libcblas, then gsl's reference gslcblas, so without the name the choice
# rests on the libcblas symlink the openblas recipe happens to add.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--with-htslib=system \
	--enable-libgsl \
	--with-cblas=openblas
make
make DESTDIR=$PKG install
