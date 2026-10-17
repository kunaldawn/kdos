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
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--with-readline=gnu \
	--without-x \
	--without-pcre \
	--without-onig \
	--with-png \
	--with-z

for mod in zlib-module.so png-module.so; do
	grep -A4 '^MODULES = ' modules/Makefile | grep -qw "$mod" || {
		echo "slang: $mod not configured" >&2
		exit 1
	}
done
make -j1 RPATH=
make DESTDIR=$PKG RPATH= install
