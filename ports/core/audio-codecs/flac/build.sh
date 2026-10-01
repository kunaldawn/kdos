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
# The internal archives (libgrabbag, libshare, …) hold LTO bytecode only, and
# binutils' ar and nm index it only through gcc's plugin: plain ar leaves the
# archive index empty and the flac link fails on every grabbag__ symbol.
export AR=gcc-ar NM=gcc-nm RANLIB=gcc-ranlib

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
