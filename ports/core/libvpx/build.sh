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

# --enable-pic and --disable-static: this is linked by ffmpeg, and a non-PIC
# static archive inside a shared library is a link failure on x86_64.
#
# --disable-unit-tests keeps a test corpus that is fetched over the network out
# of an offline build.
./configure \
	--prefix=/usr \
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
