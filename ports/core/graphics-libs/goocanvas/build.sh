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

# The Python override is PyGObject's to carry, and the demo and API reference
# are not installed. The introspection data is what a Python or Vala consumer
# binds through.
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

test -e "$PKG/usr/lib/libgoocanvas-3.0.so"
test -e "$PKG/usr/lib/girepository-1.0/GooCanvas-3.0.typelib"
