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

# fribidi and harfbuzz do the bidi and shaping work here; both are ports
# already and libass is what turns them into rendered subtitles.
# --enable-fontconfig and --enable-asm fail configure when fontconfig or nasm
# is missing instead of building a library with no font provider or no SIMD.
# libunibreak is not a port, so line breaking stays on libass's own rules.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-fontconfig --enable-asm --disable-libunibreak
make
make DESTDIR=$PKG install
