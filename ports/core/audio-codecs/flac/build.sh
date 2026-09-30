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

export CFLAGS="${CFLAGS/-O2/-O3}" CXXFLAGS="${CXXFLAGS/-O2/-O3}"

# The `flac` and `metaflac` commands are kept: they are what a terminal user
# reaches for. The examples and doxygen output are not consumed by anything
# here.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-doxygen-docs \
	--disable-examples \
	--disable-version-from-git \
	--enable-ogg
make
make DESTDIR=$PKG install
