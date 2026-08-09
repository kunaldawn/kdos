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

./autogen.sh \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--libexecdir=/usr/lib \
	--localstatedir=/var \
	--without-gnome \
	--with-gtk4=no \
	--without-libnm-glib \
	--disable-introspection \
	--disable-static
make
make DESTDIR=$PKG install
