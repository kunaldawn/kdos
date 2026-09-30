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

# --enable-https, which gnutls answers. mosquitto's http_api listener asks for
# MHD_USE_TLS whenever it is given a certfile, and a library built without TLS
# refuses the daemon outright, so that listener would not start. Named rather
# than left to the probe: a build that could not find gnutls fails here instead
# of shipping a library that turns TLS down at run time. kiwix-serve starts its
# daemon without TLS either way.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--enable-https \
	--enable-epoll=yes \
	--enable-poll=yes \
	--disable-examples \
	--disable-tools \
	--disable-curl
make
make DESTDIR=$PKG install
