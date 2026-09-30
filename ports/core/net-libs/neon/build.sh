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

# Every optional library is named. TLS is OpenSSL, verified against the
# system bundle; XML is expat. GSSAPI, NTLM, PKCS#11 (pakchois) and libproxy
# are off, and configure would otherwise enable whichever it happened to
# find. The manual pages are the ones the release tarball carries.
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
# The HTML manual is left out: it duplicates the manual pages.
make DESTDIR=$PKG install-lib install-headers install-config install-man
