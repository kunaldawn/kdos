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
# suppressions above are for. MP3 decoding comes only from libmpg123, which
# has no port, and configure stops when it is missing unless the decoder is
# disabled; the hip_* symbols then stay exported as stubs that decode nothing.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-nasm=no \
	--enable-frontend \
	--disable-decoder \
	--disable-gtktest
make
make DESTDIR=$PKG install
