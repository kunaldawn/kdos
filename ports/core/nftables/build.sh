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

# --enable-extended-parser-errors is otherwise switched on by a bison version
# probe, and --disable-debug drops the -g -DDEBUG upstream adds by default.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--with-cli=readline \
	--with-json \
	--without-xtables \
	--enable-extended-parser-errors \
	--enable-man-doc \
	--disable-debug \
	--disable-static
make
make DESTDIR=$PKG install
