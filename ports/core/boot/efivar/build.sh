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

export CFLAGS="$CFLAGS -Wno-error -Wno-address -Wno-free-nonheap-object -Wno-stringop-overflow -Wno-array-bounds"

make ERRORFILE= HOST_MARCH= libdir=/usr/lib bindir=/usr/bin mandir=/usr/share/man
make ERRORFILE= HOST_MARCH= libdir=/usr/lib bindir=/usr/bin mandir=/usr/share/man DESTDIR=$PKG install
install -Dm644 docs/efidp_make_generic.3 "$PKG/usr/share/man/man3/efidp_make_generic.3"
