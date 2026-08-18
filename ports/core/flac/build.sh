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

# The `flac` and `metaflac` commands are kept: they are what a terminal user
# reaches for. The examples and doxygen output are not consumed by anything
# here.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-doxygen-docs \
	--disable-examples \
	--enable-ogg
make
make DESTDIR=$PKG install
