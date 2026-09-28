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

# The twolame command-line encoder is built only when libsndfile is found.
# --enable-sndfile does not make its absence an error: configure warns and
# the package ships the library alone, so libsndfile stays in depends.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-maintainer-mode \
	--enable-sndfile
make
make DESTDIR=$PKG install
