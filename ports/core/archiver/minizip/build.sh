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

cd contrib/minizip
patch -p1 -i "$PORT_SRC/install-ints-header.patch"
patch -p1 -i "$PORT_SRC/zlib-1.2.8-minizip-include.patch"
autoreconf -fi
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
