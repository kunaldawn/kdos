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

make PREFIX=/usr MANPREFIX=/usr/share/man CFLAGS="$CFLAGS -std=c99" LDFLAGS="$LDFLAGS -lm"
make DESTDIR=$PKG PREFIX=/usr MANPREFIX=/usr/share/man CFLAGS="$CFLAGS -std=c99" LDFLAGS="$LDFLAGS -lm" install
install -m755 blind-rotate-90 blind-rotate-180 blind-rotate-270 "$PKG/usr/bin/"
