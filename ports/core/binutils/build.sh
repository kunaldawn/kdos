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

mkdir -v build
cd       build

../configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--disable-nls \
	--disable-werror \
	--enable-64-bit-bfd \
	--enable-deterministic-archives \
	--enable-gold \
	--enable-ld=default \
	--enable-lto \
	--enable-plugins \
	--enable-shared \
	--enable-threads \
	--with-system-zlib
make tooldir=/usr
make tooldir=/usr DESTDIR=$PKG install
