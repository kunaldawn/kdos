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

# A CROSS BINUTILS AND NOTHING MORE. --with-sysroot names where the target's
# own headers and libraries will live so the linker searches there rather than
# in the host's /usr/lib, which is musl for x86_64 and would link silently
# wrong. --disable-werror because a 2.45 binutils built by GCC 15 trips
# warnings upstream has not caught up with, and none of them is about avr.
mkdir -p build && cd build
../configure \
	--target=avr \
	--prefix=/usr \
	--with-sysroot=/usr/avr \
	--disable-nls \
	--disable-werror \
	--disable-gdb \
	--disable-sim \
	--enable-multilib \
	--with-pkgversion="KDOS"
make
make DESTDIR=$PKG install
# The info and man pages are the HOST binutils' own, byte for byte, and a
# second copy of them under a cross name is a file conflict rather than a
# document.
rm -rf "$PKG/usr/share/info" "$PKG/usr/share/man" "$PKG/usr/share/locale"
