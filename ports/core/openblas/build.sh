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

# DYNAMIC_ARCH=1 IS THE ONE DECISION IN THIS RECIPE AND IT IS THE OPPOSITE OF
# `kdos march`'s. Everywhere else this distro refuses to guess at instruction
# sets and measures instead; OpenBLAS is the case where the library ITSELF
# carries a kernel per microarchitecture and picks at RUNTIME by cpuid, which
# is the dispatch musl closes off for everything else and which upstream has
# maintained for twenty years. Without it, a package built on this machine
# runs the generic kernel on every other one — or SIGILLs, if TARGET was
# inferred.
#
# USE_OPENMP=0: there is no libgomp runtime worth pulling in for a library
# whose consumers here are single-process command-line tools, and OpenMP plus
# a threaded caller is the classic oversubscription that makes numeric code
# slower than serial. The pthread path is on.
#
# NO_LAPACK=0 and NO_LAPACKE=0 are the defaults and stay: scipy, octave and
# every solver in C2 want LAPACK, and a BLAS without it is half a package.
make \
	DYNAMIC_ARCH=1 \
	USE_OPENMP=0 \
	USE_THREAD=1 \
	NUM_THREADS=64 \
	NO_STATIC=1 \
	NO_AFFINITY=1 \
	FC=gfortran
make \
	DYNAMIC_ARCH=1 \
	USE_OPENMP=0 \
	USE_THREAD=1 \
	NUM_THREADS=64 \
	NO_STATIC=1 \
	NO_AFFINITY=1 \
	FC=gfortran \
	PREFIX=/usr DESTDIR=$PKG install

# Consumers look for `libblas`/`liblapack` by those names; upstream installs
# only libopenblas. Without these a configure script reports no BLAS on a
# machine that has one.
ln -sf libopenblas.so   $PKG/usr/lib/libblas.so
ln -sf libopenblas.so   $PKG/usr/lib/libblas.so.3
ln -sf libopenblas.so   $PKG/usr/lib/liblapack.so
ln -sf libopenblas.so   $PKG/usr/lib/liblapack.so.3
ln -sf libopenblas.so   $PKG/usr/lib/libcblas.so
