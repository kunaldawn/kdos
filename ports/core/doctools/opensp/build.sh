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


autoreconf -f -i

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--mandir=/usr/share/man \
	--disable-static \
	--disable-nls \
	--enable-http \
	--enable-default-catalog=/etc/sgml/catalog \
	--enable-default-search-path=/usr/share/sgml

make
make DESTDIR=$PKG install

rm -rf $PKG/usr/share/doc
