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
	--enable-libdaemon

make
make DESTDIR=$PKG install

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
