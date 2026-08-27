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
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-cxx \
	--disable-python \
	--disable-ruby \
	--disable-java
make
make DESTDIR=$PKG install
