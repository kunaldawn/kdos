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

export CFLAGS="${CFLAGS/-O2/-O3}" CXXFLAGS="${CXXFLAGS/-O2/-O3}"
python3 ./configure.py \
	--prefix=/usr \
	--libdir=lib \
	--mandir=/usr/share/man \
	--cc=gcc \
	--build-targets=shared,cli \
	--disable-static-library \
	--no-install-python-module \
	--without-documentation \
	--with-zlib \
	--with-bzip2 \
	--with-lzma \
	--with-os-features=getrandom
make
make DESTDIR=$PKG install
