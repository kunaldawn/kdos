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

# ogginfo/codec_skeleton.c calls utf8_decode() without including utf8.h, which
# GCC 15 rejects rather than warns about. The implicit declaration is
# compatible with the real one — `int utf8_decode(const char *, char **)`, an
# int return and two pointer arguments, all of which pass identically under the
# implicit rules — so the flag is the whole fix and no patch is needed.
export CFLAGS="$CFLAGS -Wno-implicit-function-declaration"

./configure --prefix=/usr --disable-nls
make
make DESTDIR=$PKG install
