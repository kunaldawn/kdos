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

export CFLAGS="$CFLAGS -Wno-error"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var --disable-static \
	--with-gtk=3 \
	--enable-introspection=yes \
	--enable-vala \
	--disable-dumper \
	--disable-tests \
	--disable-gtk-doc \
	HAVE_VALGRIND_TRUE='#' HAVE_VALGRIND_FALSE=''
make
make DESTDIR=$PKG install

rm -rf "$PKG/usr/libexec" "$PKG/usr/share/doc" "$PKG/usr/share/gtk-doc"
