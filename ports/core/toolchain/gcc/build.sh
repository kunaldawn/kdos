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
	CFLAGS_FOR_TARGET="${CFLAGS/-std=gnu[0-9][0-9]/}" \
	CXXFLAGS_FOR_TARGET="$CXXFLAGS"
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
