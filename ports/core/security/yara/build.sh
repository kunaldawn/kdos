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

./bootstrap.sh

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--enable-macho \
	--enable-dex \
	--enable-magic \
	--disable-cuckoo \
	--with-crypto
make yara_LDFLAGS=
make DESTDIR=$PKG yara_LDFLAGS= install
for f in "$PKG"/usr/bin/* "$PKG"/usr/lib/libyara.so.*; do
	[ -L "$f" ] && continue
	test -z "$(readelf -d "$f" | grep /var/cache)"
done
