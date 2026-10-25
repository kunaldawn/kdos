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
	--with-avahi-priv-access-group=wheel \
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
	--enable-compat-libdns_sd \
	--disable-compat-howl \
	--disable-tests \
	--enable-libdaemon

make
make DESTDIR=$PKG install
rm -rf "$PKG/run" "$PKG/var/run"
rm -f "$PKG/etc/avahi/services/ssh.service" "$PKG/etc/avahi/services/sftp-ssh.service"
install -dm755 "$PKG/var/lib/avahi-autoipd"
