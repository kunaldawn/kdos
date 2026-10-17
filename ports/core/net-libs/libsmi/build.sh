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


patch -p1 -i "$PORT_SRC/gcc14-declarations.patch"
touch lib/parser-smi.c lib/parser-smi.tab.h lib/parser-sming.c lib/parser-sming.tab.h \
	lib/parser-yang.c lib/parser-yang.tab.h
touch lib/scanner-smi.c lib/scanner-sming.c lib/scanner-yang.c

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--mandir=/usr/share/man \
	--disable-static

make
make DESTDIR=$PKG install
