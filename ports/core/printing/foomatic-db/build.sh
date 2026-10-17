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

./configure --prefix=/usr --disable-gzip-ppds --disable-ppds-to-cups
make
make DESTDIR=$PKG install
find "$PKG/usr/share/foomatic/db/source/PPD" -name '*.ppd' -exec gzip -n9 {} +

install -d "$PKG/usr/share/cups/model"
ln -s /usr/share/foomatic/db/source/PPD "$PKG/usr/share/cups/model/foomatic-db-ppds"
