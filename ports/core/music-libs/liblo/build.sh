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

# The network tests open sockets and are compiled only for `make check`; the
# tools are oscsend, oscdump and oscsendfile.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--disable-ipv6 --enable-threads --disable-doc \
	--disable-tests --disable-network-tests --enable-tools --disable-examples
make
make DESTDIR=$PKG install
