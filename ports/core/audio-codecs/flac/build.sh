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

lto="-flto=auto -frandom-seed=flac"
export CFLAGS="${CFLAGS/-O2/-O3} $lto" CXXFLAGS="${CXXFLAGS/-O2/-O3} $lto"
export LDFLAGS="$LDFLAGS -flto=auto"

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
