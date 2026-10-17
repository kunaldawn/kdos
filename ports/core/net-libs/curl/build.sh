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
	--enable-ldap \
	--enable-ldaps \
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
	--with-brotli \
	--with-zsh-functions-dir=/usr/share/zsh/site-functions \
	--without-fish-functions-dir
grep -q '^#define USE_OPENLDAP 1' lib/curl_config.h ||
	{ echo "curl: OpenLDAP not found at configure" >&2; exit 1; }
grep -q '^#define HAVE_BROTLI 1' lib/curl_config.h ||
	{ echo "curl: brotli not found at configure" >&2; exit 1; }
make
make DESTDIR=$PKG install
