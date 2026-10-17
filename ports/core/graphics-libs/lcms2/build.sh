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

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-jpeg --with-tiff --with-zlib
for _l in 'LIB_JPEG = -ljpeg' 'LIB_TIFF = -ltiff' 'LIB_ZLIB = -lz'; do
	grep -q "^$_l\$" Makefile || { echo "lcms2: $_l missing" >&2; exit 1; }
done
make
make DESTDIR=$PKG install
