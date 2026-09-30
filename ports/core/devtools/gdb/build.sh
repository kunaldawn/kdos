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

# --enable-targets=all makes one gdb read and drive every architecture bfd
# knows, which is what the arm-none-eabi, riscv64-unknown-elf and avr firmware
# the cross compilers and openocd produce needs; the native target is still
# the default. --disable-sim keeps the instruction-set simulators that the
# same switch would otherwise build for every target out of it.
#
# Every optional library is named: the ones declared are yes, so a missing one
# fails the configure, and the ones no port provides (babeltrace, guile,
# source-highlight, libipt, amd-dbgapi, and debuginfod, which elfutils is
# built without) are no, so none of them can appear by build order. Source
# colouring falls back to Pygments at run time, which is why that is declared.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--mandir=/usr/share/man \
	--infodir=/usr/share/info \
	--enable-targets=all \
	--disable-sim \
	--with-system-readline \
	--with-system-zlib \
	--with-python=/usr/bin/python3 \
	--with-expat=yes \
	--with-lzma=yes \
	--with-zstd=yes \
	--with-xxhash=yes \
	--with-babeltrace=no \
	--without-guile \
	--disable-source-highlight \
	--with-intel-pt=no \
	--with-amd-dbgapi=no \
	--with-debuginfod=no \
	--enable-tui \
	--disable-nls \
	--disable-werror
make CPPFLAGS="-DHAVE_ASM_TERMIOS_H=1 -DTCGETS2=0x802c542a -DTCSETS2=0x402c542b"
make -C gdb DESTDIR=$PKG install
make -C gdbserver DESTDIR=$PKG install
