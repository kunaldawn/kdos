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

make \
	DYNAMIC_ARCH=1 \
	TARGET=CORE2 \
	USE_OPENMP=0 \
	USE_THREAD=1 \
	NUM_THREADS=64 \
	NO_STATIC=1 \
	NO_AFFINITY=1 \
	FC=gfortran
make \
	DYNAMIC_ARCH=1 \
	TARGET=CORE2 \
	USE_OPENMP=0 \
	USE_THREAD=1 \
	NUM_THREADS=64 \
	NO_STATIC=1 \
	NO_AFFINITY=1 \
	FC=gfortran \
	PREFIX=/usr DESTDIR=$PKG install

ln -sf libopenblas.so   $PKG/usr/lib/libblas.so
ln -sf libopenblas.so   $PKG/usr/lib/libblas.so.3
ln -sf libopenblas.so   $PKG/usr/lib/liblapack.so
ln -sf libopenblas.so   $PKG/usr/lib/liblapack.so.3
ln -sf libopenblas.so   $PKG/usr/lib/libcblas.so
