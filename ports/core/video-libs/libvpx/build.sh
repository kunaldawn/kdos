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

export CFLAGS="$CFLAGS -flto=auto -frandom-seed=libvpx" CXXFLAGS="$CXXFLAGS -flto=auto -frandom-seed=libvpx"
export LDFLAGS="$LDFLAGS -flto=auto"
./configure \
	--prefix=/usr \
	--as=nasm \
	--libdir=/usr/lib \
	--enable-shared \
	--disable-static \
	--enable-pic \
	--enable-vp8 \
	--enable-vp9 \
	--enable-postproc \
	--enable-vp9-highbitdepth \
	--disable-examples \
	--disable-unit-tests \
	--disable-docs
make
make DESTDIR=$PKG install
