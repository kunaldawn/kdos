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

export XML_CATALOG_FILES=/etc/xml/catalog

# libffado is not a port and has no switch, so its probe is answered no.
# samplerate.h is probed with no switch either: libsamplerate in depends is
# what gives alsaloop its resampler.
./configure --prefix=/usr    \
            --disable-nls \
            --disable-alsaconf \
            --enable-bat   \
            --enable-alsaloop \
            --enable-nhlt \
            --enable-xmlto \
            --enable-rst2man \
            --with-curses=ncursesw \
            --with-udev-rules-dir=/lib/udev/rules.d \
            --with-systemdsystemunitdir=no \
            ac_cv_lib_ffado_ffado_streaming_init=no
make
make DESTDIR=$PKG install
install -d "$PKG/var/lib/alsa"
