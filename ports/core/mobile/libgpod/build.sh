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

patch -p1 -i "$PORT_SRC/playcounts-plist.patch"
patch -p1 -i "$PORT_SRC/plist-dict-set-item.patch"
patch -p1 -i "$PORT_SRC/gcc14.patch"
patch -p1 -i "$PORT_SRC/pkgconfig-requires-private.patch"

_mods="glib-2.0 gobject-2.0 gmodule-2.0 sqlite3 libplist-2.0"
export LIBGPOD_CFLAGS="$(pkg-config --cflags $_mods)"
export LIBGPOD_LIBS="$(pkg-config --libs $_mods)"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-gtk-doc \
	--disable-more-warnings \
	--enable-udev \
	--with-udev-dir=/usr/lib/udev \
	--with-libimobiledevice \
	--enable-libxml \
	--enable-gdk-pixbuf \
	--disable-pygobject \
	--without-hal \
	--without-python \
	--without-mono
make DATADIRNAME=share
make DATADIRNAME=share DESTDIR=$PKG install
rm "$PKG"/usr/lib/pkgconfig/libgpod-sharp.pc

rmdir "$PKG/tmp"
