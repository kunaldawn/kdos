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
	--disable-nls \
	--with-gnutls \
	--without-openssl \
	--without-libnettle \
	--without-libgmp \
	--with-libgcrypt \
	--with-libxml2 \
	--without-libexpat \
	--with-sqlite3 \
	--with-libcares \
	--with-libssh2 \
	--with-libz \
	--without-jemalloc \
	--with-ca-bundle=/etc/ssl/certs/ca-certificates.crt \
	ARIA2_STATIC=no
make
make DESTDIR=$PKG install
rm -rf "$PKG/usr/share/doc"
