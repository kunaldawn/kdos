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

# --without-brotli: brotli is built with cmake, and cmake links libcurl, so
# the dependency would close a cycle through the build tool itself.
#
# HTTP/3 is ngtcp2 over OpenSSL's QUIC API plus nghttp3; curl has no
# OpenSSL-only QUIC route, so --with-openssl alone does not give it.
./configure \
	--prefix=/usr \
	--disable-ldap \
	--disable-ldaps \
	--enable-threaded-resolver \
	--with-ca-bundle=/etc/ssl/certs/ca-certificates.crt \
	--with-openssl \
	--with-nghttp2 \
	--with-ngtcp2 \
	--with-nghttp3 \
	--with-libidn2 \
	--with-libpsl \
	--with-libssh2 \
	--with-zstd \
	--with-zsh-functions-dir=/usr/share/zsh/site-functions \
	--without-fish-functions-dir \
	--without-brotli
make
make DESTDIR=$PKG install
