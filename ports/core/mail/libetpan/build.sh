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

AUTOMAKE="automake --foreign" autoreconf -fi

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--with-openssl \
	--without-gnutls \
	--with-sasl \
	--without-curl \
	--without-expat \
	--with-zlib \
	--enable-threads \
	--enable-ipv6 \
	--disable-db \
	--disable-lmdb \
	--disable-lockfile
make
make DESTDIR=$PKG install
