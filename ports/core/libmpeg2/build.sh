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

# Alpine's patch: GCC 15 defaults to C23, where `getenv ()` declares a
# function of no arguments, and mpeg2dec's bundled getopt then conflicts with
# the C library's prototype.
patch -p1 -i "$PORT_SRC/gcc-15.patch"

# The library and the file-writing mpeg2dec only. The display outputs of the
# demonstration program are X11 and SDL 1.2, and neither is built. CPU
# acceleration is chosen at run time.
./configure --prefix=/usr --libdir=/usr/lib \
	--enable-shared \
	--disable-static \
	--with-pic \
	--without-x \
	--disable-sdl \
	--enable-accel-detect
make
make DESTDIR=$PKG install
