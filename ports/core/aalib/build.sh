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

patch -p1 -i $PORT_SRC/ncurses-opaque-window.patch

export CFLAGS="$CFLAGS -Wno-implicit-function-declaration"

./configure --prefix=/usr \
            --libdir=/usr/lib \
            --mandir=/usr/share/man \
            --infodir=/usr/share/info \
            --disable-static \
            --with-ncurses \
            --with-slang-driver=no \
            --with-x11-driver=no \
            --without-x
make
make DESTDIR=$PKG install

rm -f $PKG/usr/share/info/dir
