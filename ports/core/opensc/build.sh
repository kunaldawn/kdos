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

# openssl4.patch is upstream's commit 8ad96adc5f, which follows this release:
# OpenSSL 4 made ASN1_STRING opaque, and 0.27.1 still reads its fields
# directly in the PKCS#11 module and four tools, so without it the build stops
# at the first of them.
#
# opensc.module goes to p11-kit's module directory, so every p11-kit consumer
# — gnutls, openconnect's pkcs11: URIs — loads opensc-pkcs11.so without being
# told to; `ssh -I /usr/lib/opensc-pkcs11.so` names the module directly. The
# readers come from pcscd, dlopen'd as libpcsclite.so.1.
#
# --disable-notify: the notifications are sent through GIO's desktop
# notification portal, which no session here answers. --disable-openpace:
# OpenPACE is not a port, and without it the German eID's PACE channel and
# npa-tool are not built. The manual pages are rendered with xsltproc from
# the docbook-xsl stylesheets, named by path because configure's detection
# knows only other distributions' directories and would build no pages.
patch -p1 -i "$PORT_SRC/openssl4.patch"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-openssl \
	--enable-zlib \
	--enable-readline \
	--enable-pcsc \
	--enable-sm \
	--enable-man \
	--disable-doc \
	--disable-notify \
	--disable-openpace \
	--disable-autostart-items \
	--disable-tests \
	--disable-cmocka \
	--with-xsl-stylesheetsdir=/usr/share/xml/docbook/xsl-stylesheets-nons-1.79.2 \
	--with-pcsc-provider=libpcsclite.so.1
make
make DESTDIR=$PKG install
