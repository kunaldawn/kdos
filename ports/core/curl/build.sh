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
	--disable-ldap \
	--disable-ldaps \
	--enable-threaded-resolver \
	--with-ca-bundle=/etc/ssl/certs/ca-certificates.crt \
	--with-openssl \
	--without-brotli \
	--without-libidn2 \
	--without-libpsl \
	--without-librtmp \
	--without-nghttp2 \
	--without-zstd 
make
make DESTDIR=$PKG install
