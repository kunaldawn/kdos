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
#
# zstd and isl are both probed and neither is reachable in 02_phase2, which
# builds this recipe too: left to probe, LTO's zstd compression and Graphite
# would follow build order, and declaring zstd would pull it into the
# bootstrap. No port provides isl.
mkdir -v build
cd       build

../configure \
	--prefix=/usr \
	--libexecdir=/usr/lib \
	--enable-languages=c,c++,fortran,lto \
	--disable-bootstrap \
	--with-system-zlib \
	--without-zstd \
	--without-isl \
	--enable-linker-build-id \
	--disable-nls \
	--disable-multilib \
	--enable-threads=posix \
	--enable-__cxa_atexit \
	--enable-default-pie \
	--enable-default-ssp \
	-with-pkgversion="KDOS" \
	CFLAGS_FOR_TARGET="${CFLAGS/-std=gnu[0-9][0-9]/}"
make
make DESTDIR=$PKG install

ln -sv /usr/bin/gcc $PKG/usr/bin/cc
ln -sv /usr/lib64/libstdc++.so.6 $PKG/usr/lib/libstdc++.so.6
ln -sv /usr/lib64/libgcc_s.so.1  $PKG/usr/lib/libgcc_s.so.1

# libstdc++'s pretty-printer hook goes where gdb looks, not beside the library.
# gdb auto-loads an objfile's `-gdb.py` only from its safe path, which is
# $debugdir:$datadir/auto-load, and it finds it there under the library's own
# path; next to the library in /usr/lib64 it is declined, and every STL
# container prints as its internals. The hook finds the printers from the
# library's path, not its own, so the move does not break it.
install -d "$PKG/usr/share/gdb/auto-load/usr/lib64"
mv "$PKG"/usr/lib64/libstdc++.so.*-gdb.py "$PKG/usr/share/gdb/auto-load/usr/lib64/"
