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


# GnuTLS AND NOT OpenSSL. openconnect's DTLS support against Cisco's
# non-standard handshake is the better-tested path on GnuTLS, and gnutls is
# already the image's second TLS stack rather than a new one.
#
# THE SCRIPT IS NAMED BY ABSOLUTE PATH. openconnect execs whatever
# --with-vpnc-script says on every connect; leaving it to the default points
# it at /etc/vpnc/vpnc-script, which nothing installs.
#
# --without-openssl-version-check: the check rejects any OpenSSL it was not
# taught about, and this build does not use OpenSSL at all.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--with-gnutls \
	--without-openssl \
	--without-gssapi \
	--with-vpnc-script=/usr/share/vpnc-scripts/vpnc-script \
	--disable-nls
make
make DESTDIR=$PKG install
