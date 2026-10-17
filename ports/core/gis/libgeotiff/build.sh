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

./configure --prefix=/usr --libdir=/usr/lib --mandir=/usr/share/man \
	--disable-static \
	--with-proj=/usr \
	--with-libtiff=/usr \
	--with-jpeg=yes \
	--with-zlib=yes
make
make DESTDIR=$PKG install
test -e "$PKG/usr/include/geotiff.h"
test -e "$PKG/usr/lib/libgeotiff.so"
