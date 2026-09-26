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

# --enable-ldap builds dirmngr_ldap, which dirmngr runs for ldap:// keyservers
# and for the CRLs and certificates an X.509 directory serves. configure
# answers a missing libldap with a warning and a dirmngr without it, so the
# check after it makes that a failed build.
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
