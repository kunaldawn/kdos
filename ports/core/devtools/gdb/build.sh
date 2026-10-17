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
	--with-guile=guile-3.0 \
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
