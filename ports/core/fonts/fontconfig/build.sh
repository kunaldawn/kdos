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

# THE SYSTEM CACHE LIVES UNDER /usr, NOT /var/cache. The image and every pack
# are built with var/cache excluded, so a cache there never reaches a booted
# machine and the first fontconfig client of every live boot and every new
# user scans the whole font tree before it draws. Under /usr the cache ships
# beside the fonts it describes, and kpkg's font trigger rewrites both.
./configure --prefix=/usr        \
            --sysconfdir=/etc    \
            --localstatedir=/var \
            --with-cache-dir=/usr/lib/fontconfig/cache \
            --with-default-fonts=/usr/share/fonts \
            --disable-nls \
            --disable-docbook
make
make DESTDIR=$PKG install
