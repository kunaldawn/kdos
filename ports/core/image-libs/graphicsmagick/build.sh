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

LDFLAGS= ./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--enable-shared \
	--disable-static \
	--enable-openmp \
	--without-modules \
	--with-quantum-depth=16 \
	--with-magick-plus-plus \
	--without-perl \
	--disable-magick-compat \
	--with-threads \
	--with-bzlib \
	--with-zlib \
	--with-zstd \
	--with-lzma \
	--with-png \
	--with-jpeg \
	--with-jp2 \
	--with-jxl \
	--with-jbig \
	--with-tiff \
	--with-webp \
	--with-heif \
	--with-lcms2 \
	--with-ttf \
	--with-xml \
	--with-libzip \
	--with-gs \
	--without-fpx \
	--without-wmf \
	--without-trio \
	--without-x
make LDFLAGS="$LDFLAGS"
make LDFLAGS="$LDFLAGS" DESTDIR=$PKG install
test -e "$PKG"/usr/lib/libGraphicsMagick++.so
test -e "$PKG"/usr/lib/pkgconfig/GraphicsMagick++.pc
readelf -d "$PKG"/usr/lib/libGraphicsMagick.so | grep -q "NEEDED.*libgomp"
readelf -d "$PKG"/usr/lib/libGraphicsMagick.so | grep -q "NEEDED.*libjasper"
