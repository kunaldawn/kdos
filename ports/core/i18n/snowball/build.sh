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

patch -p1 -i "$PORT_SRC/libstemmer-library.patch"
patch -p1 -i "$PORT_SRC/stemtest-fix.patch"

make CFLAGS="$CFLAGS -fPIC"

install -Dm755 -t "$PKG/usr/bin" snowball stemwords
install -Dm644 include/libstemmer.h "$PKG/usr/include/libstemmer.h"
install -d "$PKG/usr/lib"
install -m755 libstemmer.so.$version "$PKG/usr/lib/"
ln -s libstemmer.so.$version "$PKG/usr/lib/libstemmer.so.${version%%.*}"
ln -s libstemmer.so.$version "$PKG/usr/lib/libstemmer.so"
