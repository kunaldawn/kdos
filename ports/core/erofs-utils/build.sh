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

autoreconf -f -i -s
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sbindir=/usr/sbin \
	--disable-static \
	--enable-lz4 \
	--enable-lzma \
	--enable-multithreading \
	--with-libzstd \
	--with-zlib \
	--without-xxhash \
	--without-libcurl \
	--without-openssl \
	--without-libxml2 \
	--without-json-c \
	--without-libnl3 \
	--without-uuid \
	--without-selinux \
	--disable-fuse
make
make DESTDIR=$PKG install
