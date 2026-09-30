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

# gdk-pixbuf is the office-document thumbnailer: gsf-office-thumbnailer and
# its .thumbnailer entry. Left on auto it is dropped without a word when the
# library is missing, so it is asked for by name. Gnumeric and goffice read
# the typelib through introspection.
#
# bzip2 is a link probe that --with-bz2 cannot make fatal: without it the
# library still builds and cannot open a .bz2 stream. The config header is
# checked instead.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--with-gdk-pixbuf \
	--with-bz2 \
	--enable-introspection=yes \
	--disable-gtk-doc
grep -q '^#define HAVE_BZ2 1' gsf-config.h
make
make DESTDIR=$PKG install
