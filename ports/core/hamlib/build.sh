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

# EVERY BINDING IS OFF AND THE C LIBRARY IS THE DELIVERABLE. hamlib builds
# python, tcl, perl and lua wrappers through swig; what anything on this
# machine wants is `rigctl` at a prompt and the shared library that the SDR and
# digital-mode software links.
#
# The radio is on a serial port, which is what the `dialout` group and the
# CP210x/FTDI/CH341 udev rules in this tree already cover — hamlib is the
# program those rules exist for on the radio side, exactly as picocom is on the
# console side.
# harris.h declares a `mode_t` field and includes no <sys/types.h>; glibc
# leaks that typedef through another header and musl does not, so two of the
# ~250 rig backends fail on an unknown type while every other one compiles.
export CFLAGS="$CFLAGS -include sys/types.h"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--without-cxx-binding \
	--without-python-binding \
	--without-tcl-binding \
	--without-perl-binding \
	--without-lua-binding \
	--with-libusb
make
make DESTDIR=$PKG install
