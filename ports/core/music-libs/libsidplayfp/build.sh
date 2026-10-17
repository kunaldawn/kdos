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

rm -f src/psiddrv.bin src/sidtune/sidplayer1.bin src/sidtune/sidplayer2.bin

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-usbsid=yes \
	--with-exsid=yes \
	--disable-tests
make
make DESTDIR=$PKG install
