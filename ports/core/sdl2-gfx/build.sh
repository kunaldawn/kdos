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

# SDL2 is found through pkg-config's `sdl2` (sdl2-compat's Provides), which
# the configure script asks before it looks for an sdl2-config this image does
# not ship. --disable-mmx: the MMX filters are 32-bit x86 inline assembly and
# do not assemble for x86_64.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-mmx --disable-sdltest
make
make DESTDIR=$PKG install
