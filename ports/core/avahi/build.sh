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

# Everything KDOS wants from avahi is the daemon plus the client library, so
# CUPS can discover printers. All the bindings, all the toolkits and the whole
# GObject/introspection chain are off: none of them is on the host, and
# avahi-discover et al are python.
#
# --with-distro=none stops it installing an init script for someone else's
# init system; KDOS supervises it through ksvc like everything else.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--libdir=/usr/lib \
	--with-distro=none \
	--with-avahi-user=avahi \
	--with-avahi-group=avahi \
	--disable-static \
	--disable-qt5 \
	--disable-qt6 \
	--disable-gtk \
	--disable-gtk3 \
	--disable-mono \
	--disable-monodoc \
	--disable-python \
	--disable-pygobject \
	--disable-introspection \
	--disable-libevent \
	--disable-manpages \
	--disable-xmltoman \
	--disable-libdaemon=no

make
make DESTDIR=$PKG install
