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

# configure prompts for its defaults unless every prompted value is given
# here. The library, the tools (snmpget, snmpwalk, snmptrap...), the agent
# (snmpd, snmptrapd) and the MIB files are built; the Perl and Python
# bindings, embedded Perl and the Perl scripts (mib2c, snmpconf, tkmib) are
# off. USM crypto is OpenSSL; interface and route tables come from libnl.
# PCRE is PCRE1, which is not a port, so the process-table regex match is
# off; RPM, MySQL, libwrap, libelf and systemd are off.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--mandir=/usr/share/man \
	--disable-static \
	--enable-shared \
	--with-defaults \
	--with-default-snmp-version=3 \
	--with-sys-contact=root@localhost \
	--with-sys-location=unknown \
	--with-logfile=/var/log/snmpd.log \
	--with-persistent-directory=/var/lib/net-snmp \
	--with-openssl \
	--enable-blumenthal-aes \
	--enable-ipv6 \
	--with-nl \
	--without-pcre \
	--without-rpm \
	--without-elf \
	--without-mysql \
	--without-libwrap \
	--without-systemd \
	--without-perl-modules \
	--disable-embedded-perl \
	--without-python-modules \
	--disable-scripts
grep -q '^#define HAVE_LIBCRYPTO 1' include/net-snmp/net-snmp-config.h
grep -q '^#define HAVE_LIBNL3 1' include/net-snmp/net-snmp-config.h
make
# The install rules race each other under a parallel make.
make -j1 DESTDIR=$PKG install

# NOTHING STARTS snmpd. /etc/snmp/snmpd.conf does not exist until the
# administrator writes one; the agent then runs as `snmpd -f`.
install -d -m 700 "$PKG/var/lib/net-snmp"
