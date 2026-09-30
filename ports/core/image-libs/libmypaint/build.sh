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

# The brush engine alone: GIMP and Krita drive it through their own surfaces,
# so the GEGL surface stays off. i18n stays off: the only strings are brush
# setting names, and the tree carries English. Nothing reads the typelib.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-i18n \
	--disable-gegl \
	--disable-openmp \
	--disable-docs \
	--disable-introspection \
	--with-glib
make
make DESTDIR=$PKG install
