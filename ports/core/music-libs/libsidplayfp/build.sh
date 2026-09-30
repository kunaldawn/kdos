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

# The three 6502 player routines ship assembled beside their .a65 sources and
# are regenerated only when the source is newer. Removing the assembled copies
# makes the build run xa over the sources, so what the library embeds is what
# this tree assembled.
rm -f src/psiddrv.bin src/sidtune/sidplayer1.bin src/sidtune/sidplayer2.bin

# reSIDfp is found through pkg-config and is not a configure switch: without
# libresidfp the library builds with only the SIDLite engine, and a consumer
# built against 3.x that asks for reSIDfp (audacious-plugins' sid) finds
# nothing. USBSID-Pico goes through libusb and exSID through libftdi1, which
# the internal driver opens with dlopen; both are named so a missing library
# stops configure instead of dropping the device.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-usbsid=yes \
	--with-exsid=yes \
	--disable-tests
make
make DESTDIR=$PKG install
