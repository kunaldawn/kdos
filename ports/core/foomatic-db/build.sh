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

# The manufacturer PPDs are 690 MB as plain text. They are compressed here with
# gzip -n rather than by upstream's install, which runs a bare gzip: that
# records every file's time and name, and the package would differ on each
# build. cups reads the .ppd.gz files as they are.
./configure --prefix=/usr --disable-gzip-ppds --disable-ppds-to-cups
make
make DESTDIR=$PKG install
find "$PKG/usr/share/foomatic/db/source/PPD" -name '*.ppd' -exec gzip -n9 {} +

# cups-driverd lists every PPD under its model directory, following links.
install -d "$PKG/usr/share/cups/model"
ln -s /usr/share/foomatic/db/source/PPD "$PKG/usr/share/cups/model/foomatic-db-ppds"
