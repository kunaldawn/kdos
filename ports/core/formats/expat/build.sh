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

./buildconf.sh
# The entropy source is pinned to what musl has: getrandom and getentropy, and
# no arc4random. --without-docbook installs the xmlwf.1 the tarball ships
# rather than regenerating it with whichever docbook tool happens to exist.
./configure \
	--prefix=/usr \
	--with-getrandom \
	--with-getentropy \
	--without-arc4random \
	--without-arc4random-buf \
	--without-docbook \
	--without-examples \
	--without-tests
make
make DESTDIR=$PKG install
