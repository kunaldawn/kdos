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

# The release carries the C, the VAPI and the GIR that valac generated, so
# the library compiles without valac; the typelib is compiled from that GIR.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--enable-introspection=yes \
	--disable-doc \
	--disable-benchmark
make
make DESTDIR=$PKG install
