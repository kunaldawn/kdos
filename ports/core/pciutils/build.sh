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

 make OPT="${CFLAGS} -fPIC -DPIC" \
      PREFIX=/usr                \
      SHAREDIR=/usr/share/hwdata \
      MANDIR=/usr/share/man\
      SHARED=yes

 make PREFIX=/usr                \
      SHAREDIR=/usr/share/hwdata \
      MANDIR=/usr/share/man\
      SHARED=yes                 \
      DESTDIR=$PKG\
      install install-lib

 chmod -v 755 $PKG/usr/lib/libpci.so
