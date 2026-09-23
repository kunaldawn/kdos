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

# --without-openssl: musl has no SHA2 or HMAC, and configure falls back to
# libcrypto for them whenever a --prefix other than / is given. The pin keeps
# the bundled implementations, and a root network daemon off libcrypto,
# whatever the prefix.
./configure --libexecdir=/lib/dhcpcd \
            --dbdir=/var/lib/dhcpcd \
            --privsepuser=dhcpcd \
            --with-udev \
            --without-openssl
make
make DESTDIR=$PKG install
