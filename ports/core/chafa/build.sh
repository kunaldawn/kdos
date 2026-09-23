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
	--mandir=/usr/share/man \
	--disable-static \
	--disable-man \
	--disable-gtk-doc \
	--with-jpeg \
	--with-svg \
	--with-tiff \
	--with-webp \
	--with-heif \
	--without-avif \
	--without-jxl
make
make DESTDIR=$PKG install
install -Dm644 docs/chafa.1 -t "$PKG/usr/share/man/man1"
