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
# --enable-libunibreak breaks a long line where Unicode allows it (UAX #14)
# rather than only at ASCII spaces, which is what wraps a CJK or Thai subtitle
# at all; like the other two, it fails configure when the library is missing.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-fontconfig --enable-asm --enable-libunibreak
make
make DESTDIR=$PKG install
