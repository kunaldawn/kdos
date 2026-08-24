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
patch -p1 -i $PORT_SRC/alsa-ring-latency.patch

# ALSA is the only output KDOS has on a bare TTY, and --disable-dl links
# libasound instead of dlopening it, so a missing ALSA is a link error here
# rather than a silent "no sound" at runtime.
./configure --prefix=/usr \
            --sysconfdir=/etc \
            --libdir=/usr/lib \
            --mandir=/usr/share/man \
            --disable-static \
            --disable-alldrv \
            --enable-alsa \
            --disable-dl
make
make DESTDIR=$PKG install

rm -f $PKG/usr/share/info/dir
