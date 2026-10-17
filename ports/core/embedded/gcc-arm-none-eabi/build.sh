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

mkdir -p build && cd build
../configure \
	--target=arm-none-eabi \
	--prefix=/usr \
	--libexecdir=/usr/lib \
	--with-sysroot=/usr/arm-none-eabi \
	--enable-languages=c,c++ \
	--without-headers \
	--with-newlib \
	--disable-nls \
	--disable-shared \
	--disable-threads \
	--disable-libssp \
	--disable-libgomp \
	--disable-libquadmath \
	--disable-libatomic \
	--disable-libstdcxx-pch \
	--disable-decimal-float \
	--with-gnu-as --with-gnu-ld \
	--with-zstd=/usr \
	--without-isl \
	--with-multilib-list=rmprofile \
	--enable-host-pie \
	--enable-host-bind-now \
	--with-pkgversion="KDOS" \
	CFLAGS_FOR_TARGET="-Os -pipe -ffunction-sections -fdata-sections"
make all-gcc all-target-libgcc LIBGCC2_DEBUG_CFLAGS=-g0
make DESTDIR=$PKG install-gcc install-target-libgcc LIBGCC2_DEBUG_CFLAGS=-g0
rm -rf "$PKG/usr/share/info" "$PKG/usr/share/man/man7"
