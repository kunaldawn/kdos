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

# ALSA is the only output KDOS has on a bare TTY, and --disable-dl links
# libasound instead of dlopening it, so a missing ALSA is a link error here
# rather than a silent "no sound" at runtime. ALSA's default device already
# reaches PipeWire, so the PulseAudio driver would be a second path to the
# same server.
#
# --disable-alldrv also turns off the drivers that need no library at all:
# the wav, raw and aiff file writers and the stdout and pipe outputs are what
# let a module be rendered to a file or piped to another player, so they are
# named back on.
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
