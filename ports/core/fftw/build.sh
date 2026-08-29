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
# --enable-sse2/avx are RUNTIME-DISPATCHED by fftw itself, not baked in: it
# checks cpuid and picks a codelet, which is the same property that makes
# OpenBLAS's DYNAMIC_ARCH safe here.
COMMON="--prefix=/usr --libdir=/usr/lib --disable-static --enable-shared
        --enable-threads --enable-sse2 --enable-avx --enable-avx2"

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
	--enable-threads --enable-long-double
make
make DESTDIR=$PKG install
