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
export AR=gcc-ar NM=gcc-nm RANLIB=gcc-ranlib

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-doxygen-docs \
	--disable-examples \
	--disable-version-from-git \
	--enable-ogg
make
make DESTDIR=$PKG install
