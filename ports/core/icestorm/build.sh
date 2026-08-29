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

# THIS IS THE DATABASE, AND nextpnr IS USELESS WITHOUT IT. The iCE40 chip
# database reverse-engineered here is what tells the placer what the fabric
# looks like; nextpnr's ice40 target reads it at build time, so this port has
# to be installed BEFORE nextpnr is compiled, not merely before it is run.
#
# iceprog is the programmer and it talks to an FTDI chip — which is why
# libftdi is a dependency and why the `dialout` udev rules this tree already
# ships are what make it usable without root.
make PREFIX=/usr
make PREFIX=/usr DESTDIR=$PKG install
