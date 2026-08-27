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

# A FREESTANDING CROSS COMPILER: --without-headers and --with-newlib together
# are what tell gcc there is no libc yet, so libgcc is built without reaching
# for <stdio.h>. The C library is a SEPARATE port built with this compiler —
# picolibc for the 32-bit targets, avr-libc for AVR — and that ordering is why
# the two cannot be one recipe.
#
# ONLY all-gcc AND all-target-libgcc ARE BUILT. A plain `make` here tries to
# build the target libstdc++ as well, which needs the libc that does not exist
# yet and fails a long way in.
#
# Every runtime gcc would normally add is off: libssp, libgomp, libquadmath,
# libatomic and shared libgcc all assume a hosted target. On a Cortex-M there
# is no OS to host them.
mkdir -p build && cd build
../configure \
	--target=riscv64-unknown-elf \
	--prefix=/usr \
	--libexecdir=/usr/lib \
	--with-sysroot=/usr/riscv64-unknown-elf \
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
	--disable-multilib --with-abi=lp64d --with-arch=rv64gc \
	--with-pkgversion="KDOS"
make all-gcc all-target-libgcc
make DESTDIR=$PKG install-gcc install-target-libgcc
# The host gcc's own documentation, installed a second time under a cross name.
rm -rf "$PKG/usr/share/info" "$PKG/usr/share/man" "$PKG/usr/share/locale"
