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

./autogen.sh

# ALSA ONLY, AND DELIBERATELY NOT PULSE. espeak-ng has to speak on a bare tty1
# where no session bus and no pipewire exist — that is the case a screen reader
# is for — and the pulse backend would make the library prefer a server that is
# not running. On a desktop session pipewire's own ALSA compatibility layer is
# what carries it, so nothing is lost at the other end.
./configure --prefix=/usr --libdir=/usr/lib --disable-static --without-pulseaudio
make
make DESTDIR=$PKG install
