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
	--mandir=/usr/share/man \
	--disable-wordview \
	--enable-langinfo \
	--with-output=utf-8
grep -q '^#define HAVE_LANGINFO 1' src/config.h
make
install -d "$PKG/usr/share/man/man1"
make -j1 installroot="$PKG" install
test -x "$PKG/usr/bin/catdoc"
test -x "$PKG/usr/bin/xls2csv"
test -x "$PKG/usr/bin/catppt"
