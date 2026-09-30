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

# The makefiles add -Werror ahead of CFLAGS; gcc's newer warnings in this
# 2016 code would otherwise stop the build.
export CFLAGS="$CFLAGS -Wno-error"

# configure declares the HAVE_VALGRIND conditional only when the tests are
# enabled and then refuses to finish because it was never defined; the two
# values settle it with the tests off.
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

# dbusmenu-bench, installed whatever the flags, is a profiling script for the
# unversioned `python`, which does not exist here; the documentation directory
# holds only its readme.
rm -rf "$PKG/usr/libexec" "$PKG/usr/share/doc" "$PKG/usr/share/gtk-doc"
