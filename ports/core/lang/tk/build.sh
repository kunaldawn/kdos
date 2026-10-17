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

cd unix

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--mandir=/usr/share/man \
	--with-tcl=/usr/lib \
	--with-x \
	--enable-64bit \
	--enable-shared \
	--disable-rpath \
	--enable-xft \
	--enable-xss \
	--enable-libcups \
	--disable-zipfs
make
make DESTDIR=$PKG install

make DESTDIR=$PKG install-private-headers

ln -sf wish9.0 $PKG/usr/bin/wish
readelf -d "$PKG/usr/lib/libtcl9tk9.0.so" | grep -q "NEEDED.*libXss"
