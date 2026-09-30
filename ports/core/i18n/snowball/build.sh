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

# Upstream's makefile builds libstemmer as a static archive only; the patch
# adds the shared library with an exported-symbol map, as Alpine ships it.
patch -p1 -i "$PORT_SRC/libstemmer-library.patch"
patch -p1 -i "$PORT_SRC/stemtest-fix.patch"

# The makefile assigns CFLAGS itself, which hides the environment's; on the
# command line they reach every object, and -fPIC is what the shared library
# needs from each of them.
make CFLAGS="$CFLAGS -fPIC"

install -Dm755 -t "$PKG/usr/bin" snowball stemwords
install -Dm644 include/libstemmer.h "$PKG/usr/include/libstemmer.h"
install -d "$PKG/usr/lib"
install -m755 libstemmer.so.$version "$PKG/usr/lib/"
ln -s libstemmer.so.$version "$PKG/usr/lib/libstemmer.so.${version%%.*}"
ln -s libstemmer.so.$version "$PKG/usr/lib/libstemmer.so"
