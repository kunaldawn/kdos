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
	--libdir=/usr/lib \
	--bindir=/usr/bin \
	--disable-static \
	--disable-backtrace \
	--disable-python \
	--enable-documentation \
	--enable-convert \
	--with-convert=ext2 \
	--enable-zoned \
	--enable-zstd \
	--enable-lzo \
	--enable-libudev \
	--with-crypto=builtin

_sphinx="-D extensions=sphinx.builders.manpage"
make O="$_sphinx"
make DESTDIR=$PKG O="$_sphinx" install
