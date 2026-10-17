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
	--libdir=/usr/lib \
	--disable-static \
	--enable-shared \
	--disable-nls \
	--enable-threadsafe-ssl=posix \
	--with-ssl=openssl \
	--with-ca-bundle=/etc/ssl/certs/ca-certificates.crt \
	--with-expat \
	--with-zlib \
	--without-gssapi \
	--without-libntlm \
	--without-pakchois \
	--without-libproxy
grep -q '^#define NE_HAVE_SSL 1' config.h
grep -q '^#define NE_HAVE_DAV 1' config.h
make
make DESTDIR=$PKG install-lib install-headers install-config install-man
