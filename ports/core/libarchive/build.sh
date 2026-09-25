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
	--enable-bsdtar \
	--enable-bsdcpio \
	--enable-bsdcat \
	--enable-bsdunzip \
	--enable-acl \
	--enable-xattr \
	--enable-posix-regex-lib=libc \
	--with-zlib \
	--with-bz2lib \
	--with-lzma \
	--with-zstd \
	--with-lz4 \
	--with-openssl \
	--with-expat \
	--without-xml2 \
	--without-libb2 \
	--with-lzo2 \
	--without-nettle \
	--without-mbedtls
make
make DESTDIR=$PKG install
