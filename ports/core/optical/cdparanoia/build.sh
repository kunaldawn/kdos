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

patch -p1 -i "$PORT_SRC/format-security.patch"
patch -p1 -i "$PORT_SRC/gcc.patch"
patch -p1 -i "$PORT_SRC/fix-includes.patch"

env -u CC ./configure --prefix=/usr --sysconfdir=/etc
make -j1 CC="${CC:-cc} $LDFLAGS"
make -j1 prefix=$PKG/usr MANDIR=$PKG/usr/share/man install
