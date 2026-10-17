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

LDFLAGS= ./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--enable-libcurl \
	--enable-s3 \
	--enable-gcs \
	--enable-ref-cache \
	--enable-bz2 \
	--enable-lzma \
	--with-libdeflate \
	--disable-plugins
make LDFLAGS="$LDFLAGS"
make LDFLAGS="$LDFLAGS" DESTDIR=$PKG install
