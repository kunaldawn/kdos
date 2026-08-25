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

# SQL OVER LOG LINES IS WHY THIS IS HERE AND NOT JUST less. Offline you cannot
# paste a log into a search box, so the machine has to be able to answer a
# question about its own logs — and lnav's sqlite view is the only thing on
# this system that can. libarchive is what lets it read a rotated .gz or .xz
# without unpacking it first.
./configure --prefix=/usr --disable-static
make
make DESTDIR=$PKG install
