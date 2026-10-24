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
	ac_cv_path_PSDelegate=gs \
	--prefix=/usr \
	--sysconfdir=/etc \
	--mandir=/usr/share/man \
	--disable-docs \
	--without-gcc-arch \
	--without-modules \
	--with-threads \
	--with-png \
	--with-jpeg \
	--with-tiff \
	--with-webp \
	--with-zlib \
	--with-bzlib \
	--with-lzma \
	--with-zstd \
	--with-zip \
	--with-xml \
	--with-freetype \
	--with-fontconfig \
	--with-pango \
	--without-x \
	--without-perl \
	--with-fftw \
	--with-djvu \
	--without-dmr \
	--without-fpx \
	--without-gslib \
	--without-gvc \
	--with-heic \
	--with-jbig \
	--with-jxl \
	--with-lcms \
	--without-lqr \
	--with-openexr \
	--with-openjp2 \
	--with-raqm \
	--with-raw \
	--with-rsvg \
	--without-uhdr \
	--without-wmf
make
make DESTDIR=$PKG install
test -z "$(grep '<include' "$PKG/etc/ImageMagick-7/type.xml")"
rm "$PKG"/etc/ImageMagick-7/type-*.xml
test ! -e "$PKG/usr/share/doc/ImageMagick-7/www"
