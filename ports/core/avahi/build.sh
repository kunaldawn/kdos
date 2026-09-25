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

# The source is the release-candidate tag's archive, which is the repository
# as committed: no configure script and no prebuilt manual pages. autoreconf
# makes the one, and xmltoman the others from man/*.xml, which configure refuses
# to go on without once the pages are not prebuilt.
#
# Everything KDOS wants from avahi is the daemon plus the client library, so
# CUPS can discover printers. The client library IS the D-Bus API, so dbus is
# on by name, as are expat for the service files, gdbm for the service-type
# database, and the glib main-loop adapter cups-browsed builds on together with
# the gobject wrapper that comes with glib.
# The bindings, the toolkits and introspection are off: none of them is on the
# host, and avahi-discover et al are python. Every one of them is named, on or
# off, so the result does not depend on what else was built first.
#
# --with-distro=none stops it installing an init script for someone else's
# init system, and --with-systemdsystemunitdir=no stops it asking pkg-config
# for a unit directory; KDOS supervises it through ksvc like everything else.
#
# avahi-autoipd, the IPv4 link-local client for a cable between two machines
# with no DHCP server, drops to an account of its own and chroots into
# /var/lib/avahi-autoipd, which it gives to that account. Both names are the
# ones postinstall.sh makes; a name with no account is a daemon that refuses to
# start.
autoreconf -fi
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--libdir=/usr/lib \
	--with-distro=none \
	--with-systemdsystemunitdir=no \
	--with-xml=expat \
	--with-avahi-user=avahi \
	--with-avahi-group=avahi \
	--with-autoipd-user=avahi-autoipd \
	--with-autoipd-group=avahi-autoipd \
	--disable-static \
	--enable-dbus \
	--enable-gdbm \
	--enable-glib \
	--enable-gobject \
	--disable-qt3 \
	--disable-qt4 \
	--disable-qt5 \
	--disable-gtk \
	--disable-gtk3 \
	--disable-mono \
	--disable-monodoc \
	--disable-python \
	--disable-pygobject \
	--disable-introspection \
	--disable-libevent \
	--disable-libsystemd \
	--disable-doxygen-doc \
	--disable-compat-libdns_sd \
	--disable-compat-howl \
	--disable-tests \
	--enable-libdaemon

make
make DESTDIR=$PKG install
rm -rf "$PKG/run"
