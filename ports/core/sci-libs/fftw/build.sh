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

FORTRAN="--enable-fortran F77=gfortran"
COMMON="--prefix=/usr --libdir=/usr/lib --disable-static --enable-shared
        --enable-threads --enable-openmp --enable-sse2 --enable-avx --enable-avx2
        --enable-avx512 $FORTRAN"

./configure $COMMON
make
make DESTDIR=$PKG install
make clean

./configure $COMMON --enable-float
make
make DESTDIR=$PKG install
make clean

./configure --prefix=/usr --libdir=/usr/lib --disable-static --enable-shared \
	--enable-threads --enable-openmp --enable-long-double $FORTRAN
make
make DESTDIR=$PKG install
