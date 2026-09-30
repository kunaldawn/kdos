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

# PROJ resolves the EPSG codes a GeoTIFF names; libgeotiff 1.7 refuses to
# configure without it. JPEG and zlib are linked beside libtiff for the
# compressed rasters geotifcp copies.
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
