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

sed -i 's/PKG_INSTALLDIR//' configure.ac
sed -i '/pkgconfig_DATA/i pkgconfigdir = $(libdir)/pkgconfig' Makefile.am
./bootstrap.sh
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
    --libdir=/usr/lib

make
make DESTDIR=$PKG install
