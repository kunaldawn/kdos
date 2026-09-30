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

# --with-htslib=system, OR THERE ARE TWO OF THEM. samtools bundles a copy of
# htslib and builds it by default; the result is a binary linked against a
# different htslib from bcftools', which is exactly the version mismatch the
# htslib recipe's own comment warns about — arrived at by accident rather than
# by a bump.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--with-htslib=system
make
make DESTDIR=$PKG install
