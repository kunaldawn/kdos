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


./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-gengetopt \
	--disable-doxygen \
	--with-opensp-includes=/usr/include/OpenSP \
	--with-opensp-libs=/usr/lib

make CXXFLAGS="-DIN_LIBOFX -fvisibility=hidden -fvisibility-inlines-hidden $CXXFLAGS -std=gnu++17"
make DESTDIR=$PKG install

rm -rf $PKG/usr/share/doc
