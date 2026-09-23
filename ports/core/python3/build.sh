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
	--enable-ipv6 \
	--enable-loadable-sqlite-extensions \
	--enable-optimizations \
	--enable-shared \
	--with-computed-gotos \
	--with-lto \
	--with-system-expat \
	--with-system-libmpdec \
	--with-tzpath=/usr/share/zoneinfo
make EXTRA_CFLAGS="$CFLAGS"
make EXTRA_CFLAGS="$CFLAGS" DESTDIR=$PKG install maninstall

# Remove test files
rm -rf $PKG/usr/lib/python${_version}/test
