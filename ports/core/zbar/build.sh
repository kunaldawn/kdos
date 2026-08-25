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

autoreconf -f -i

# EVERY GUI FRONT END IS OFF. zbar ships GTK, Qt, Java and Python bindings and
# an X overlay; the host has none of those by rule, and what is wanted here is
# zbarimg and zbarcam — a file or a camera in, text out. `--with-x=no` matters
# even with the toolkits off, or configure links the X overlay for zbarcam.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--without-gtk \
	--without-qt \
	--without-java \
	--without-python \
	--without-x \
	--with-imagemagick
make
make DESTDIR=$PKG install
