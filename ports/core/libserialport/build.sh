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

# IT ENUMERATES, WHICH termios DOES NOT. Opening /dev/ttyUSB0 is trivial;
# finding out that it exists, that it is an FTDI, and which of three identical
# adapters it is, needs sysfs walking that every bench program would otherwise
# reimplement. libsigrok and half the flashers link this rather than doing so.
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
