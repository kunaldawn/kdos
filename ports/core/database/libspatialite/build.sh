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

patch -p1 -i "$PORT_SRC/libxml2-no-http.patch"

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-mathsql \
	--enable-proj \
	--enable-iconv \
	--enable-freexl=yes \
	--enable-epsg \
	--enable-geos \
	--enable-gcp \
	--enable-geosadvanced \
	--enable-geosreentrant \
	--enable-rttopo \
	--enable-libxml2 \
	--enable-minizip \
	--enable-geopackage \
	--enable-examples=no
make
make DESTDIR=$PKG install
test -e "$PKG"/usr/lib/mod_spatialite.so
