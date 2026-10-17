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

touch ChangeLog
gtkdocize

autoreconf -fi

./configure --prefix=/usr --libdir=/usr/lib \
	--disable-static \
	--disable-introspection \
	--disable-vala \
	--disable-gtk-doc \
	--enable-crypto \
	--with-libidn
make
make DESTDIR=$PKG install
