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

export CFLAGS="$CFLAGS -D_GNU_SOURCE"

./configure --prefix=/usr --libdir=/usr/lib \
	--enable-reentrant --enable-shared --disable-static --disable-curl \
	--with-bzip2 F77=gfortran
make
make DESTDIR=$PKG install
rm -f "$PKG/usr/bin/cookbook" "$PKG/usr/bin/smem" "$PKG/usr/bin/speed"
