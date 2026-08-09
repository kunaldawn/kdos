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

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--disable-logind \
	--disable-nsl \
	--disable-static \
	--enable-lastlog \
	--with-yescrypt \
	--without-libbsd \
	--without-libpam \
	--with-group-name-max-length=32 

make
make DESTDIR=$PKG install

mkdir -p $PKG/bin $PKG/etc/cron/daily
install -m 755 $PORT_SRC/pwck $PKG/etc/cron/daily
