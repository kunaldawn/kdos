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

./configure --prefix=/usr --libdir=/usr/lib --disable-static
_amcflags="-Wall -DGIT_REVISION=\\\"$version\\\""
make AM_CFLAGS="$_amcflags"

make DESTDIR=$PKG install-libLTLIBRARIES install-hdrHEADERS install-pkgconfigDATA
make -C examples DESTDIR=$PKG AM_CFLAGS="$_amcflags" install-binPROGRAMS

test -f "$PKG/usr/lib/libperseus-sdr.so"
test -f "$PKG/usr/include/perseus-sdr.h"
test -x "$PKG/usr/bin/perseustest"
