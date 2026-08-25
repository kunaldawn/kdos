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

autoreconf -f -i

# `-t ANSI` draws the code out of block characters, which is the whole reason
# this belongs on a machine whose desktop is a cell grid: a Syncthing device
# id, a WiFi credential or an F-Droid URL can leave this machine through a
# phone camera with no network between them.
./configure --prefix=/usr --libdir=/usr/lib --disable-static --without-tools=NO
make
make DESTDIR=$PKG install
