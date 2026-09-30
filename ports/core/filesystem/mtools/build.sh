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

# floppyd is a remote floppy server linked against libX11 for its X
# authority check, and configure builds it whenever it finds X headers, so it
# is named off and the X probe with it. Its two manual pages install
# regardless, so they are removed with the program they describe.
./configure --prefix=/usr \
            --sysconfdir=/etc/default \
            --disable-floppyd \
            --without-x
make
make DESTDIR=$PKG install
rm -f $PKG/usr/share/man/man1/floppyd.1 $PKG/usr/share/man/man1/floppyd_installtest.1
