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

# No pulse, no esd, no arts: ALSA is what this host has, and an --enable for a
# backend that is not here builds a plugin that fails at open() rather than at
# configure.
./autogen.sh
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-alsa --disable-pulse --disable-esd --disable-arts --disable-nas
make
make DESTDIR=$PKG install
