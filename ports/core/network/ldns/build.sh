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

LDFLAGS= ./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--disable-static \
	--with-drill \
	--with-examples \
	--with-ssl=/usr \
	--with-ca-file=/etc/ssl/cert.pem \
	--enable-sha2 \
	--enable-ecdsa \
	--enable-ed25519 \
	--enable-ed448 \
	--enable-dane \
	--enable-dane-verify \
	--enable-dane-ta-usage \
	--disable-gost \
	--disable-dsa
grep -q '^LIBPCAP_LIBS *= *-lpcap' Makefile \
	|| { echo "ldns: libpcap missing, ldns-dpa would be skipped" >&2; exit 1; }
make LDFLAGS="$LDFLAGS"
make LDFLAGS="$LDFLAGS" DESTDIR=$PKG install
