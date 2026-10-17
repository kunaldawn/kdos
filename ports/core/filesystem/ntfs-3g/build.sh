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
	--exec-prefix=/usr \
	--libdir=/usr/lib \
	--sbindir=/usr/sbin \
	--mandir=/usr/share/man \
	--disable-static \
	--disable-ldconfig \
	--enable-extras \
	--enable-posix-acls \
	--enable-xattr-mappings \
	--enable-crypto \
	--with-uuid \
	--without-hd \
	--with-fuse=internal
make
make DESTDIR=$PKG install

rm -f $PKG/usr/lib/*.la
