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

# fortran is in the language list because the numeric ring is built on it —
# LAPACK, the ODE and FFT libraries and octave all want a Fortran compiler, and
# there is no separate gfortran tarball to add later. It is paid twice: phase 2
# rebuilds this port with itself before phase 3 builds it again.
mkdir -v build
cd       build

../configure \
	--prefix=/usr \
	--libexecdir=/usr/lib \
	--enable-languages=c,c++,fortran,lto \
	--disable-bootstrap \
	--with-system-zlib \
	--disable-nls \
	--disable-multilib \
	--enable-threads=posix \
	--enable-__cxa_atexit \
	--enable-default-pie \
	--enable-default-ssp \
	-with-pkgversion="KDOS"
make
make DESTDIR=$PKG install

ln -sv /usr/bin/gcc $PKG/usr/bin/cc
ln -sv /usr/lib64/libstdc++.so.6 $PKG/usr/lib/libstdc++.so.6
ln -sv /usr/lib64/libgcc_s.so.1  $PKG/usr/lib/libgcc_s.so.1
