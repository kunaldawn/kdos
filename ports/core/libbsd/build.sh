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

# musl does not have off64_t, but off_t is 64-bit
export CFLAGS="$CFLAGS -Doff64_t=off_t"
./configure --prefix=/usr
make
make DESTDIR=$PKG install

# for musl libc extensions
mkdir -p $PKG/usr/include/sys
cp $PKG/usr/include/bsd/sys/cdefs.h $PKG/usr/include/sys/
cp $PKG/usr/include/bsd/sys/queue.h $PKG/usr/include/sys/
cp $PKG/usr/include/bsd/sys/tree.h  $PKG/usr/include/sys/

# Fix recursion in cdefs.h (since it is now the system cdefs.h)
sed -i 's|#include <sys/cdefs.h>|/* #include <sys/cdefs.h> */|g' $PKG/usr/include/sys/cdefs.h
