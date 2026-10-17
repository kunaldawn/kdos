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

libtoolize
install -d m4
aclocal -I m4
autoheader
automake -c -a --foreign
autoconf

./configure \
	--prefix=/usr \
	--with-cyrus-sasl=/usr \
	--disable-static

make
make DESTDIR=$PKG install

rm -f "$PKG/usr/lib/sasl2/libxoauth2.la"
