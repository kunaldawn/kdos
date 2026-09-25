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
#
# sigrok-firmware-fx2lafw is in depends for the fx2lafw driver: an FX2 board
# has no firmware of its own, and the driver uploads fx2lafw-*.fw from
# /usr/share/sigrok-firmware on every plug-in.
#
# VXI-11, the LAN transport of networked scopes, supplies and meters, is built
# only when configure can link clnt_create from <rpc/rpc.h>. musl has no Sun
# RPC, so libtirpc provides it, and configure's probe finds it only through
# these flags. The library compiles -std=c99, which hides the BSD integer
# types tirpc's headers use; _DEFAULT_SOURCE exposes them. A probe that fails
# drops VXI and the build still succeeds, so the result is checked.
export CPPFLAGS="$CPPFLAGS -D_DEFAULT_SOURCE $(pkg-config --cflags libtirpc)"
export LIBS="$LIBS $(pkg-config --libs libtirpc)"
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
grep -q 'define HAVE_RPC 1' config.h || {
	echo 'libsigrok: <rpc/rpc.h> not linkable, VXI-11 would be missing' >&2
	exit 1
}
make
make DESTDIR=$PKG install

# contrib/60-libsigrok.rules sets ENV{ID_SIGROK}="1" on every USB device a
# driver here supports and grants nothing; make install leaves it out, so it is
# installed by hand. fs/etc/udev/rules.d/70-kdos-sigrok.rules turns the mark
# into the dialout grant. 61-libsigrok-plugdev.rules and
# 61-libsigrok-uaccess.rules stay out: the one names a group this system does
# not have and the other a tag nothing here consumes.
install -Dm644 contrib/60-libsigrok.rules "$PKG/usr/lib/udev/rules.d/60-libsigrok.rules"
