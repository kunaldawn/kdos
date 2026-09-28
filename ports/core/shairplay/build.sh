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

# Kodi's own depends patch: dns_sd.h is required only where there is no
# dlopen. The library dlopens libdns_sd.so for its advertising at run time,
# so building it needs no Bonjour header; Kodi advertises AirPlay through its
# own Avahi client and never calls that path.
patch -p1 -i "$PORT_SRC/configure-dns-sd-check.patch"
autoreconf -fi

# The library and its headers only. The shairplay program links libao when
# configure finds it, and reads airport.key from its working directory, so as
# an installed command it cannot start; it is not built.
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make -C include
make -C src/lib
make -C include DESTDIR=$PKG install
make -C src/lib DESTDIR=$PKG install

test -e "$PKG"/usr/lib/libshairplay.so
test -e "$PKG"/usr/include/shairplay/raop.h
