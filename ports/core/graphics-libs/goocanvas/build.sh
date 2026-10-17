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

export CFLAGS="$CFLAGS -Wno-implicit-function-declaration -Wno-implicit-int \
	-Wno-int-conversion -Wno-incompatible-pointer-types"
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--disable-static \
	--disable-nls \
	--disable-rpath \
	--enable-introspection=yes \
	--enable-python=no \
	--disable-gtk-doc
make
make DESTDIR=$PKG install
rm -rf "$PKG/usr/share/gtk-doc"

test -e "$PKG/usr/lib/libgoocanvas-3.0.so"
test -e "$PKG/usr/lib/girepository-1.0/GooCanvas-3.0.typelib"
