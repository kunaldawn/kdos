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

# --without-zstd BECAUSE THIS RECIPE IS BUILT IN 02_phase2, where zstd does not
# exist: declaring it would drag zstd, xz, lz4 and pkgconf into the bootstrap,
# and forcing it on there fails without pkg-config. Left at auto it would
# follow build order instead, so it is off everywhere and the ld is the same
# in every phase. xxhash is a header with no dependencies, so the inline hash
# ld uses for string merging costs the bootstrap one small port.
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
