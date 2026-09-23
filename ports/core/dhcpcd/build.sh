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

# --without-openssl: musl has no SHA2 or HMAC, so configure links libcrypto
# whenever it is installed. The bundled implementations cover what the daemon
# needs and keep a root network daemon off libcrypto.
./configure --libexecdir=/lib/dhcpcd \
            --dbdir=/var/lib/dhcpcd \
            --privsepuser=dhcpcd \
            --with-udev \
            --without-openssl
make
make DESTDIR=$PKG install
