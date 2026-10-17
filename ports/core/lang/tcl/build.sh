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

cd unix

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--mandir=/usr/share/man \
	--enable-64bit \
	--with-system-libtommath \
	--with-tzdata=no \
	--disable-rpath
LD_LIBRARY_PATH="$PWD${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" make
make DESTDIR=$PKG install

make DESTDIR=$PKG install-private-headers

ln -sf tclsh9.0 $PKG/usr/bin/tclsh
