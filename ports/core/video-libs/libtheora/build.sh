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

export CFLAGS="${CFLAGS/-O2/-O3}"

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-examples --disable-spec --disable-doc
make
make DESTDIR=$PKG install
rm -r "$PKG/usr/share/doc/libtheora/html" "$PKG/usr/share/doc/libtheora/latex" \
	"$PKG/usr/share/doc/libtheora/doxygen-build.stamp"
