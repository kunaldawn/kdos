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

./configure --prefix=/usr     	\
            --bindir=/bin     	\
            --sysconfdir=/etc   \
            --disable-nls
make
make DESTDIR=$PKG install

install -Dm644 "$SRC/doc/COPYING" "$PKG/usr/share/licenses/$name/COPYING"
install -Dm644 "$SRC/doc/COPYING.LGPL" "$PKG/usr/share/licenses/$name/COPYING.LGPL"
