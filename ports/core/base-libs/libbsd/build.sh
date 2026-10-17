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

export CFLAGS="$CFLAGS -Doff64_t=off_t"
./configure --prefix=/usr
make
make DESTDIR=$PKG install

mkdir -p $PKG/usr/include/sys
cp $PKG/usr/include/bsd/sys/cdefs.h $PKG/usr/include/sys/
cp $PKG/usr/include/bsd/sys/queue.h $PKG/usr/include/sys/
cp $PKG/usr/include/bsd/sys/tree.h  $PKG/usr/include/sys/

patch -d "$PKG" -p0 -i "$PORT_SRC/sys-cdefs-no-self-include.patch"
