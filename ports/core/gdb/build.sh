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
	--with-system-readline \
	--with-python=/usr/bin/python3 \
	--enable-tui \
	--disable-nls \
	--disable-werror
make CPPFLAGS="-DHAVE_ASM_TERMIOS_H=1 -DTCGETS2=0x802c542a -DTCSETS2=0x402c542b"
make DESTDIR=$PKG install
