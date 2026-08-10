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
