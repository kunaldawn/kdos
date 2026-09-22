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
	--enable-ld=default \
	--enable-lto \
	--enable-plugins \
	--enable-shared \
	--with-system-zlib
make tooldir=/usr
make tooldir=/usr DESTDIR=$PKG install
