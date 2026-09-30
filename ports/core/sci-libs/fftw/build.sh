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

# THREE PRECISIONS, THREE BUILDS, AND ALL THREE ARE NEEDED. fftw compiles for
# ONE precision per configure run and installs a differently-suffixed library
# each time: double is what octave and scipy want, single is what every SDR and
# audio path wants (half the memory traffic and the accuracy is irrelevant at
# 8-bit ADC samples), and long double is what a few of the C2 solvers ask for.
# A build that ran configure once produces a package whose consumers half fail
# to link.
#
# --enable-sse2/avx/avx2/avx512 are RUNTIME-DISPATCHED by fftw itself, not
# baked in: it checks cpuid (and XGETBV for the register state) and picks a
# codelet, which is the same property that makes OpenBLAS's DYNAMIC_ARCH safe
# here.
#
# The Fortran wrappers are a C shim that only needs gfortran's name mangling.
# Left to search, configure drops them silently when no Fortran compiler
# answers; with F77 named it fails instead ("cannot compile a simple Fortran
# program"), so a gcc built without fortran cannot ship a package missing them.
FORTRAN="--enable-fortran F77=gfortran"
COMMON="--prefix=/usr --libdir=/usr/lib --disable-static --enable-shared
        --enable-threads --enable-sse2 --enable-avx --enable-avx2
        --enable-avx512 $FORTRAN"

./configure $COMMON
make
make DESTDIR=$PKG install
make clean

./configure $COMMON --enable-float
make
make DESTDIR=$PKG install
make clean

# Long double has no SIMD path at all — the vector units do not carry 80-bit
# floats — so those flags are dropped rather than ignored with a warning.
./configure --prefix=/usr --libdir=/usr/lib --disable-static --enable-shared \
	--enable-threads --enable-long-double $FORTRAN
make
make DESTDIR=$PKG install
