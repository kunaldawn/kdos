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
	--disable-static \
	--enable-dbus \
	--enable-gdbm \
	--disable-dbm \
	--enable-glib \
	--enable-gobject \
	--disable-qt3 \
	--disable-qt4 \
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
	--disable-xmltoman \
	--disable-doxygen-doc \
	--disable-compat-libdns_sd \
	--disable-compat-howl \
	--disable-tests \
	--enable-libdaemon

make
make DESTDIR=$PKG install
rm -rf "$PKG/run"

# A PUBLIC HEADER MUST BE VALID UTF-8, and upstream's is ISO-8859-1: an "á"
# in a comment in avahi-common/domain.h. Anything that reads a header AS TEXT
# rather than as bytes then fails — brltty's Tcl dependency scanner opens every
# include it follows and stops on `invalid or incomplete multibyte or wide
# character`, from a file it never named. Re-encoding is lossless and is done
# here rather than worked around in each consumer.
find "$PKG" -name '*.h' | while read -r h; do
	iconv -f UTF-8 -t UTF-8 "$h" >/dev/null 2>&1 && continue
	iconv -f ISO-8859-1 -t UTF-8 "$h" > "$h.utf8" && mv -f "$h.utf8" "$h"
done
