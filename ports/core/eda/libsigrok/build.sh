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

export CPPFLAGS="$CPPFLAGS -D_DEFAULT_SOURCE $(pkg-config --cflags libtirpc)"
export LIBS="$LIBS $(pkg-config --libs libtirpc)"
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--with-libserialport \
	--with-libftdi \
	--with-libhidapi \
	--with-libbluez \
	--with-libusb \
	--with-libgio \
	--without-librevisa \
	--without-libgpib \
	--without-libieee1284 \
	--disable-cxx \
	--disable-python \
	--disable-ruby \
	--disable-java
grep -q 'define HAVE_RPC 1' config.h || {
	echo 'libsigrok: <rpc/rpc.h> not linkable, VXI-11 would be missing' >&2
	exit 1
}
make
make DESTDIR=$PKG install

install -Dm644 contrib/60-libsigrok.rules "$PKG/usr/lib/udev/rules.d/60-libsigrok.rules"
