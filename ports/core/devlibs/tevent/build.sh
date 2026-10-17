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

./configure --prefix=/usr --libdir=/usr/lib \
	--disable-python \
	--disable-rpath \
	--disable-rpath-install \
	--bundled-libraries='!talloc,cmocka' \
	--builtin-libraries=replace
make
make DESTDIR=$PKG install
rm -r "$PKG/usr/lib/tevent"
test -e "$PKG/usr/lib/pkgconfig/tevent.pc"
