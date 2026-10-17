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

./configure --libexecdir=/lib/dhcpcd \
            --dbdir=/var/lib/dhcpcd \
            --privsepuser=dhcpcd \
            --with-udev \
            --with-hooks=ntp.conf \
            --with-eghooks=yp.conf \
            --without-openssl
make
make DESTDIR=$PKG install
