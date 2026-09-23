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
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-nasm=no \
	--enable-frontend \
	--enable-decoder \
	--disable-gtktest
make
make DESTDIR=$PKG install
