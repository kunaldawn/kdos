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

autoreconf -fi

export XML_CATALOG_FILES=/etc/xml/catalog
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-official-khronos-headers \
	--disable-custom-vendordir \
	--disable-custom-layerdir \
	--enable-debug \
	--enable-pthread-once \
	--disable-update-database
make
make DESTDIR=$PKG install

rm -rf "$PKG/usr/share/doc"
