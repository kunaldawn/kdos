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

# Every power helper is built: httppower and redfishpower (curl, jansson)
# and snmppower (net-snmp). genders and TCP wrappers are not ports and are
# off. configure refuses to run without flex and bison, which build the
# configuration grammar.
#
# Upstream compiles with -Werror, and musl's <sys/poll.h> warns that the
# header is a redirect; CFLAGS follows upstream's flags and keeps every
# warning a warning.
export CFLAGS="$CFLAGS -Wno-error"
# The daemon's headers use struct timeval without including <sys/time.h>,
# which glibc's headers pull in and musl's do not.
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

# NOTHING STARTS powermand. /etc/powerman/powerman.conf.example stays an
# example; the daemon's socket directory is /var/run/powerman, a path
# through the /var/run link that a package cannot own, so it goes.
rm -rf "$PKG/var/run"
