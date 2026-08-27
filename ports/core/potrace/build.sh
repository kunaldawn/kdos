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

# THE ONE DIRECTION NOTHING ELSE HERE GOES. Every other image tool on this
# machine rasterises — turns curves into pixels; potrace goes the other way,
# which is what makes a scanned logo, a photographed sketch or a screenshot of
# a diagram into something a laser cutter, a CNC or a vector editor can use.
# On a bench with a plotter attached that is the whole workflow.
./configure --prefix=/usr --libdir=/usr/lib --with-libpotrace --disable-static
make
make DESTDIR=$PKG install
