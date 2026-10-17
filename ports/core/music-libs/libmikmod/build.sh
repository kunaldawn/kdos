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

patch -p1 -i $PORT_SRC/alsa-null-close.patch
patch -p1 -i $PORT_SRC/alsa-nonblocking-update.patch

./configure --prefix=/usr \
            --sysconfdir=/etc \
            --libdir=/usr/lib \
            --mandir=/usr/share/man \
            --disable-static \
            --disable-alldrv \
            --enable-alsa \
            --enable-wav \
            --enable-raw \
            --enable-aiff \
            --enable-stdout \
            --enable-pipe \
            --enable-threads \
            --disable-dl
make
make DESTDIR=$PKG install

rm -f $PKG/usr/share/info/dir
