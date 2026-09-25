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
	--enable-lib-only \
	--disable-cryptotest \
	--with-openssl \
	--without-gnutls \
	--without-boringssl \
	--without-picotls \
	--without-wolfssl \
	--without-libnghttp3 \
	--without-libev \
	--without-jemalloc \
	--without-libbrotlienc \
	--without-libbrotlidec
make
make DESTDIR=$PKG install
