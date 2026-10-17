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

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--with-gnutls \
	--without-openssl \
	--with-gssapi \
	--with-gnutls-tss2=tss2-esys \
	--with-libpcsclite \
	--without-stoken \
	--without-libproxy \
	--without-libpskc \
	--with-builtin-json \
	--with-external-browser=/usr/bin/xdg-open \
	--with-vpnc-script=/usr/share/vpnc-scripts/vpnc-script \
	--disable-nls
make
make DESTDIR=$PKG install
