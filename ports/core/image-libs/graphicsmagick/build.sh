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

# Magick++ is what Octave's imread, imwrite and imfinfo are built on: Octave
# needs the ImageMagick 6 API (Magick::PixelPacket), which GraphicsMagick
# keeps and ImageMagick 7 removed. A 16-bit quantum gives Octave's uint16
# images their full range. The coders are linked into the library rather
# than loaded as modules. Every format with a library here is on; FlashPIX,
# WMF and TRIO have none and are off. The X11 display and animate tools are
# not built, and the ImageMagick-named compatibility commands are not
# installed, so nothing collides with the imagemagick port.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
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
make
make DESTDIR=$PKG install
test -e "$PKG"/usr/lib/libGraphicsMagick++.so
test -e "$PKG"/usr/lib/pkgconfig/GraphicsMagick++.pc
