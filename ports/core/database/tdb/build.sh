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

./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
	--disable-python \
	--disable-rpath \
	--disable-rpath-install \
	--builtin-libraries=replace
make
make DESTDIR=$PKG install
test -x "$PKG/usr/bin/tdbtool"
test -e "$PKG/usr/share/man/man8/tdbtool.8"
