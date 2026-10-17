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

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--with-gtk \
	--with-librsvg \
	--with-lasem=yes \
	--with-config-backend=gsettings \
	--enable-introspection=yes \
	--disable-gtk-doc
grep -q '^#define GOFFICE_WITH_EPS 1' goffice/goffice-config.h
make
make DESTDIR=$PKG install
