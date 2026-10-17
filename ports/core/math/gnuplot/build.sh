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

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--with-readline=gnu \
	--with-qt=qt6 \
	--without-wx \
	--without-x \
	--with-bitmap-terminals \
	--with-gd \
	--with-cairo \
	--with-lua \
	--with-libcerf \
	--without-amos \
	--without-caca \
	--without-latex \
	--enable-history-file
for def in QTTERM HAVE_LIBCERF; do
	grep -q "^#define $def 1" config.h || { echo "gnuplot: $def not configured" >&2; exit 1; }
done
make
make DESTDIR=$PKG install
