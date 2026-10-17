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

cd gettext-runtime
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-rpath \
	--disable-java \
	--disable-csharp \
	--without-emacs \
	--enable-nls \
	--with-included-libintl
make -C intl
make -C intl DESTDIR=$PKG install

rm -f "$PKG/usr/include/libintl.h"
