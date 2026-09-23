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

# frontend/get_audio.c is pre-C99 in ways GCC 14
# promotes from warning to error. The whole family is suppressed at once
# rather than one flag per round trip — the rule ports/core/aalib writes down.
export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
	-Wno-int-conversion -Wno-incompatible-pointer-types"

# --enable-nasm is deliberately OFF: lame's hand-written assembly predates
# x86_64 and the C path is what every distribution ships. The frontend is the
# `lame` command itself, named explicitly because it is what the pre-C99
# suppressions above are for. The decoder is libmpg123: it gives `lame
# --decode`, MP3 input and a working hip_* API, and configure stops rather than
# build without it. The frontend's VBR histogram draws through ncurses, which
# configure links whenever it finds it.
#
# THE STATIC LIBRARY IS BUILT FOR THE FRONTEND AND NOT SHIPPED. With the
# decoder on, `lame` calls hip_set_pinfo and hip_finish_pinfo, which
# libmp3lame.sym leaves out of the shared library's exports, so linking the
# frontend against the .so fails with an undefined reference. configure links
# the frontends with libtool's -static unless --enable-dynamic-frontends is
# given, so with the archive built `lame` takes libmp3lame from it, and the
# archive carries every symbol.
./configure --prefix=/usr --libdir=/usr/lib --enable-static \
	--enable-nasm=no \
	--enable-frontend \
	--enable-decoder \
	--disable-gtktest
make
make DESTDIR=$PKG install
rm -f "$PKG/usr/lib/libmp3lame.a"
