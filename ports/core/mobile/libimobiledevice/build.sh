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

# gcc 14 makes int-conversion an error, and the afc and lockdown sources
# still pass an int where a pointer is declared.
export CFLAGS="$CFLAGS -Wno-error=int-conversion"

# --with-openssl names the TLS backend. configure takes OpenSSL only while
# neither --with-gnutls nor --with-mbedtls is given, so the flag keeps the
# choice from following whichever library a later edit mentions.
# --without-cython: the Python binding needs libplist's Cython headers, which
# the libplist port does not build.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-openssl \
	--without-cython
make
make DESTDIR=$PKG install
