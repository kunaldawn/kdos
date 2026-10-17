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

make CFLAGS="$CFLAGS" LIBS="$LDFLAGS -lm -lz -lpthread"
install -Dm755 minimap2 $PKG/usr/bin/minimap2
install -Dm644 minimap2.1 $PKG/usr/share/man/man1/minimap2.1
install -Dm755 misc/paftools.js $PKG/usr/share/minimap2/paftools.js
