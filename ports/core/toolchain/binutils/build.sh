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
	--sysconfdir=/etc \
	--disable-nls \
	--disable-werror \
	--enable-64-bit-bfd \
	--enable-deterministic-archives \
	--enable-ld=default \
	--enable-lto \
	--enable-plugins \
	--enable-shared \
	--with-system-zlib \
	--with-xxhash \
	--without-zstd \
	--without-debuginfod \
	--without-msgpack
make tooldir=/usr
make tooldir=/usr DESTDIR=$PKG install

install -m644 ../binutils/doc/{addr2line,ar,c++filt,elfedit,nm,objcopy,objdump,ranlib,readelf,size,strings,strip}.1 \
	../gas/doc/as.1 -t "$PKG/usr/share/man/man1"
rm -f "$PKG"/usr/share/man/man1/{dlltool,windres,windmc}.1
