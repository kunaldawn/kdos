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
	--localstatedir=/var \
	--libexecdir=/usr/lib/$name \
	--enable-card-support \
	--enable-ccid-driver \
	--enable-sqlite \
	--enable-gnutls \
	--disable-ntbtls \
	--with-tss=intel \
	--with-readline \
	--with-zlib \
	--with-bzip2 \
	--disable-ldap \
	--disable-nls \
	--disable-tests
make
make DESTDIR=$PKG install
