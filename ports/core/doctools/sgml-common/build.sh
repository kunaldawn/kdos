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

cp --remove-destination /usr/share/automake-*/install-sh .
cp --remove-destination /usr/share/automake-*/missing .
cp --remove-destination /usr/share/automake-*/mkinstalldirs .
./configure --prefix=/usr --sysconfdir=/etc --mandir=/usr/share/man
make
make docdir=/usr/share/doc DESTDIR=$PKG install

install -Dm644 "/usr/share/licenses/automake/COPYING" "$PKG/usr/share/licenses/$name/COPYING"
