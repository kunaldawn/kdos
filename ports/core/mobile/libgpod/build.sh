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

# configure asks pkg-config for "libplist", a name libplist 2 no longer
# installs (it is libplist-2.0). Handing the module set over in LIBGPOD_CFLAGS
# and LIBGPOD_LIBS skips that lookup; without them configure stops.
_mods="glib-2.0 gobject-2.0 gmodule-2.0 sqlite3 libplist-2.0"
export LIBGPOD_CFLAGS="$(pkg-config --cflags $_mods)"
export LIBGPOD_LIBS="$(pkg-config --libs $_mods)"

# The udev callouts write SysInfoExtended when an iPod is plugged in; newer
# models refuse a database written without it. The rules go under
# /usr/lib/udev, where eudev reads them. libusb reads that file from a nano
# 5G and has no switch: it is found or the read is dropped. sg3_utils, the
# SCSI path for older models, has no port, so that path is absent.
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
make
make DESTDIR=$PKG install

# install makes the udev callout's mount parent, /tmp, which the base
# filesystem already owns.
rmdir "$PKG/tmp"
