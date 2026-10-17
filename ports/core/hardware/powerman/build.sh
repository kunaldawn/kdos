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

export CFLAGS="$CFLAGS -Wno-error"
export CPPFLAGS="$CPPFLAGS -include sys/time.h"
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--mandir=/usr/share/man \
	--disable-static \
	--with-httppower \
	--with-redfishpower \
	--with-snmppower \
	--without-genders \
	--without-tcp-wrappers \
	--without-systemdsystemunitdir
make
make DESTDIR=$PKG install

rm -rf "$PKG/var/run"
