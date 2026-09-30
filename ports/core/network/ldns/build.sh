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

# --with-drill is what makes this a command and not only a library: dnsmasq is
# shipped and without drill there is no way to ask this machine a DNS question.
#
# --with-examples IS THE ldns-* TOOLS, not samples: ldns-keygen, ldns-signzone,
# ldns-verify-zone, ldns-dane, ldns-walk and the rest are the DNSSEC and DANE
# commands distributions ship as ldns-tools. ldns-dpa among them reads packet
# captures, which is what libpcap is for.
#
# ldns-dane's CA store is the bundle ca-certificates installs; this image has
# no hashed certificate directory for a CA path to point at.
#
# EVERY ALGORITHM IS NAMED. SHA-2, ECDSA, Ed25519, Ed448 and DANE verification
# are on and fail configure if openssl lacks them. GOST is off: it needs an
# OpenSSL GOST engine, which OpenSSL 4 cannot load, and RFC 8624 deprecates
# it for DNSSEC. DSA is off: RFC 8624 forbids validating with it.
#
# NO DEFAULT TRUST ANCHOR EXISTS. The compiled-in path is
# /etc/unbound/root.key and no port installs one, so `drill -S` needs `-k`.
./configure \
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
make
make DESTDIR=$PKG install
