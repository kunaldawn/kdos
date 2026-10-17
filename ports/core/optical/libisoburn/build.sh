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

./configure --prefix=/usr \
	--disable-debug \
	--enable-libreadline \
	--disable-libedit \
	--enable-libacl \
	--enable-zlib \
	--disable-libjte \
	--disable-libcdio \
	--enable-external-filters \
	--enable-launch-frontend
make
make -j1 DESTDIR=$PKG install

ln -s xorriso "$PKG/usr/bin/mkisofs"
ln -s xorriso "$PKG/usr/bin/genisoimage"
