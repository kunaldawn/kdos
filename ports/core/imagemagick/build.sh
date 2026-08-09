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
	--sysconfdir=/etc \
	--mandir=/usr/share/man \
	--without-modules \
	--with-threads \
	--with-png \
	--with-jpeg \
	--with-tiff \
	--with-webp \
	--with-zlib \
	--with-xml \
	--with-freetype \
	--with-fontconfig \
	--with-pango \
	--without-x \
	--without-perl \
	--without-fftw \
	--without-djvu \
	--without-fpx \
	--without-gslib \
	--without-gvc \
	--without-heic \
	--without-jxl \
	--without-lcms \
	--without-lqr \
	--without-openexr \
	--without-openjp2 \
	--without-raqm \
	--without-raw \
	--without-rsvg \
	--without-wmf
make
make DESTDIR=$PKG install
