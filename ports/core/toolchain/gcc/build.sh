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

patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/t-linux64-lib.patch"

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
	--enable-host-pie \
	--enable-host-bind-now \
	-with-pkgversion="KDOS" \
	CFLAGS_FOR_TARGET="${CFLAGS/-std=gnu[0-9][0-9]/}" \
	CXXFLAGS_FOR_TARGET="$CXXFLAGS" \
	LDFLAGS_FOR_TARGET="$LDFLAGS"
make LIBGCC2_DEBUG_CFLAGS=-g0
make DESTDIR=$PKG install LIBGCC2_DEBUG_CFLAGS=-g0

install -m644 ../gcc/doc/{gcc,g++,gfortran}.1 -t "$PKG/usr/share/man/man1"
install -d "$PKG/usr/share/info"
install -m644 ../gcc/doc/{gcc,gccint,cpp,cppinternals}.info ../gcc/fortran/gfortran.info \
	-t "$PKG/usr/share/info"

ln -sv /usr/bin/gcc $PKG/usr/bin/cc

install -d "$PKG/usr/share/gdb/auto-load/usr/lib"
mv "$PKG"/usr/lib/libstdc++.so.*-gdb.py "$PKG/usr/share/gdb/auto-load/usr/lib/"
