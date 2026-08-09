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

scratch isinstalled libxslt && OPTS="--enable-manpages"

autoreconf -f -i -s
./configure $OPTS \
	--prefix=/usr \
	--bindir=/sbin \
	--sbindir=/sbin \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--libexecdir=/lib \
	--with-rootprefix= \
	--with-rootlibdir=/lib
make

mkdir -pv $PKG/lib/udev/rules.d
mkdir -pv $PKG/etc/udev/rules.d

make DESTDIR=$PKG install
