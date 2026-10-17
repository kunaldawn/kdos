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

patch -p1 -i "$PORT_SRC/configure-dns-sd-check.patch"
autoreconf -fi

./configure --prefix=/usr --libdir=/usr/lib --disable-static
make -C include
make -C src/lib
make -C include DESTDIR=$PKG install
make -C src/lib DESTDIR=$PKG install

test -e "$PKG"/usr/lib/libshairplay.so
test -e "$PKG"/usr/include/shairplay/raop.h
