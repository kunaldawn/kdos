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

autoreconf -f -i

export CFLAGS="${CFLAGS/-O2/-O3}" CXXFLAGS="${CXXFLAGS/-O2/-O3}"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-examples \
	--enable-openmp \
	--enable-jpeg \
	--enable-lcms \
	--enable-zlib
make
make DESTDIR=$PKG install
grep -q -- -fopenmp "$PKG"/usr/lib/pkgconfig/libraw.pc
