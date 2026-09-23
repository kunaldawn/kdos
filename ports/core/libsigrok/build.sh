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


# EVERY LANGUAGE BINDING IS OFF AND THE DRIVER SET IS THE POINT. The value here
# is a hundred hardware drivers behind one interface — the £15 logic analyser,
# the bench multimeter with a serial cable, the USB scope — so that a program
# reads samples without knowing which box produced them.
#
# --disable-cxx and the rest: the bindings need swig and a C++ ABI, and
# sigrok-cli is C. PulseView is the GUI and is Qt, so it is not here.
#
# Each --with-<lib> turns a missing library into a configure error. Left to
# detection, a missing one drops every driver that needs it and the build still
# succeeds: libusb carries fx2lafw and the Saleae/DSLogic family, libserialport
# and hidapi the serial meters, bluez and gio the BLE ones, libftdi the
# FTDI-based analysers. librevisa, libgpib and libieee1284 are not ports, so
# they are named off rather than left to whatever the build host has.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--with-libserialport \
	--with-libftdi \
	--with-libhidapi \
	--with-libbluez \
	--with-libusb \
	--with-libgio \
	--without-librevisa \
	--without-libgpib \
	--without-libieee1284 \
	--disable-cxx \
	--disable-python \
	--disable-ruby \
	--disable-java
make
make DESTDIR=$PKG install
