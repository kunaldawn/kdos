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

export CFLAGS="$CFLAGS -g0" CXXFLAGS="$CXXFLAGS -g0"

_conf="--prefix=/usr --libdir=/usr/lib --disable-static --without-x
	--enable-xspice --enable-cider --enable-openmp --disable-debug
	--with-fftw3=yes"
mkdir build-shared build-program
_cm=(-C src/xspice/icm LDFLAGS="-shared $LDFLAGS")
(cd build-shared && ../configure $_conf --with-ngshared && make && rm -f src/xspice/icm/*/*.cm &&
	make "${_cm[@]}" && make DESTDIR=$PKG install)
(cd build-program && ../configure $_conf --with-readline=yes && make && rm -f src/xspice/icm/*/*.cm &&
	make "${_cm[@]}" && make DESTDIR=$PKG install)
