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

patch -p1 -i "$PORT_SRC/import-ecdh-key.patch"
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--libexecdir=/usr/lib/$name \
	--enable-card-support \
	--enable-ccid-driver \
	--enable-sqlite \
	--enable-gnutls \
	--disable-ntbtls \
	--with-tss=intel \
	--with-readline \
	--with-zlib \
	--with-bzip2 \
	--enable-ldap \
	--disable-nls \
	--disable-tests
grep -q '^#define USE_LDAP 1' config.h ||
	{ echo "gnupg: OpenLDAP not found at configure" >&2; exit 1; }
make
make DESTDIR=$PKG install
