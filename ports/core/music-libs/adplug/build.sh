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

# adplugdb compiles its default database path from sharedstatedir; left at the
# configure default it would look in /usr/com/adplug, a directory no system
# has. Nothing is installed there, the path is only where adplugdb looks.
./configure --prefix=/usr --libdir=/usr/lib --sharedstatedir=/var/lib --disable-static
make
make DESTDIR=$PKG install
