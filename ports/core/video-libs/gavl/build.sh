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

patch -p1 -i "$PORT_SRC/no-x87-fpmath.patch"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--mandir=/usr/share/man \
	--disable-static \
	--without-doxygen \
	--with-cpuflags=none
make -C gavl LDFLAGS="$LDFLAGS"
make -C include
make -C gavl DESTDIR=$PKG install
make -C include DESTDIR=$PKG install
make DESTDIR=$PKG install-pkgconfigDATA
