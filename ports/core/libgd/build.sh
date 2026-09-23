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

patch -p1 -i $PORT_SRC/heif-stride.patch

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-rpath \
	--disable-werror \
	--enable-gd-formats \
	--without-x \
	--with-zlib \
	--with-png \
	--with-jpeg \
	--with-webp \
	--with-tiff \
	--with-heif \
	--with-freetype \
	--with-fontconfig \
	--with-raqm \
	--with-liq \
	--without-xpm \
	--without-avif \
	ax_cv_c_openmp=unknown
make
make DESTDIR=$PKG install
