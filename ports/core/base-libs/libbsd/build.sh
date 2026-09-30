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

# The copy becomes the system sys/cdefs.h, so its include of <sys/cdefs.h>
# would include itself.
patch -d "$PKG" -p0 -i "$PORT_SRC/sys-cdefs-no-self-include.patch"
